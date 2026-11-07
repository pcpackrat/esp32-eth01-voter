#include "AudioCodec.h"
#include <driver/i2s_std.h>
#include <esp_timer.h>

static i2s_chan_handle_t s_txChan = nullptr;
static i2s_chan_handle_t s_rxChan = nullptr;
static int32_t s_txBuf[AUDIO_FRAME_SAMPLES * 2]; // stereo frames, sample in the top 16 bits

// Capture: the RX DMA ISR copies each completed buffer into s_ring, indexed
// by the same running sample count it timestamps. Every sample's index and
// capture time come from one place - no driver queue in between, so nothing
// can slip (the driver's own RX queue is never read).
#define RX_RING 4096 // mono samples, power of 2 (~0.5 s)
static int16_t s_ring[RX_RING];
static portMUX_TYPE s_dmaMux = portMUX_INITIALIZER_UNLOCKED;
static uint64_t s_dmaCount = 0; // samples written to s_ring (ISR only writes)
static int64_t s_dmaUs = 0;     // esp_timer time of the latest completion
static TaskHandle_t s_reader = nullptr;
static uint64_t s_readIdx = 0;
static volatile uint32_t s_rxOverruns = 0;
static int16_t s_slotPeak[2] = {0, 0}; // diagnostic, ISR writes
static uint32_t s_nonzero = 0, s_total = 0;

static bool IRAM_ATTR onRxDone(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user) {
  int64_t now = esp_timer_get_time();
  // 32-bit slots: the codec's 16-bit sample is the top half of each word
  const int32_t *src = (const int32_t *)event->dma_buf;
  size_t frames = event->size / (2 * sizeof(int32_t)); // stereo 32-bit
  uint64_t base = s_dmaCount;
  for (size_t i = 0; i < frames; i++)
    s_ring[(base + i) & (RX_RING - 1)] = (int16_t)(src[i * 2 + AUDIO_RX_SLOT] >> 16);
  portENTER_CRITICAL_ISR(&s_dmaMux);
  for (size_t i = 0; i < frames * 2; i++) {
    int16_t v = (int16_t)(src[i] >> 16);
    int16_t a = (v == -32768) ? 32767 : (v < 0 ? -v : v);
    if (a > s_slotPeak[i & 1])
      s_slotPeak[i & 1] = a;
    if (src[i] != 0) // any bit, including the low half
      s_nonzero++;
  }
  s_total += frames * 2;
  s_dmaCount = base + frames;
  s_dmaUs = now;
  portEXIT_CRITICAL_ISR(&s_dmaMux);
  BaseType_t woken = pdFALSE;
  if (s_reader)
    vTaskNotifyGiveFromISR(s_reader, &woken);
  return woken == pdTRUE;
}

void AudioCodec::dmaSnapshot(uint64_t *count, int64_t *us) {
  portENTER_CRITICAL(&s_dmaMux);
  *count = s_dmaCount;
  *us = s_dmaUs;
  portEXIT_CRITICAL(&s_dmaMux);
}

uint32_t AudioCodec::rxOverflows() const { return s_rxOverruns; }

void AudioCodec::slotPeaks(int16_t *left, int16_t *right, uint32_t *nonzero, uint32_t *total) {
  portENTER_CRITICAL(&s_dmaMux);
  *left = s_slotPeak[0];
  *right = s_slotPeak[1];
  *nonzero = s_nonzero;
  *total = s_total;
  s_slotPeak[0] = s_slotPeak[1] = 0;
  s_nonzero = s_total = 0;
  portEXIT_CRITICAL(&s_dmaMux);
}

bool AudioCodec::begin(int bclk, int ws, int dout, int din) {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  // 1 ms DMA buffers: RX audio reaches the GPS framer within ~1 ms of
  // capture (a master's send latency is part of every site's TX timing).
  // 64 buffers = 64 ms of slack each way.
  chanCfg.dma_desc_num = 64;
  chanCfg.dma_frame_num = AUDIO_DMA_SAMPLES;
  chanCfg.auto_clear = true; // TX underrun plays silence, not stale audio

  esp_err_t err = i2s_new_channel(&chanCfg, &s_txChan, &s_rxChan);
  if (err != ESP_OK) {
    Serial.printf("[Audio] i2s_new_channel failed: %d\r\n", err);
    return false;
  }

  i2s_std_config_t stdCfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                      I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED, // codec clocks from BCLK
              .bclk = (gpio_num_t)bclk,
              .ws = (gpio_num_t)ws,
              .dout = (gpio_num_t)dout,
              .din = (gpio_num_t)din,
              .invert_flags = {false, false, false},
          },
  };
  // 32-bit slots: BCLK = 64 fs = 512 kHz. The codec makes its clock from
  // BCLK, and its x4/x8 multiplier won't run from 256 kHz. The codec still
  // sends/takes 16-bit samples, in the top half of each slot.
  stdCfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
  stdCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  err = i2s_channel_init_std_mode(s_txChan, &stdCfg);
  if (err == ESP_OK)
    err = i2s_channel_init_std_mode(s_rxChan, &stdCfg);
  if (err != ESP_OK) {
    Serial.printf("[Audio] i2s_channel_init_std_mode failed: %d\r\n", err);
    return false;
  }

  i2s_event_callbacks_t rxCbs = {};
  rxCbs.on_recv = onRxDone; // the driver's RX queue is unused (it just drops old entries)
  i2s_channel_register_event_callback(s_rxChan, &rxCbs, nullptr);

  // Preload TX with silence so the first enable doesn't play garbage
  memset(s_txBuf, 0, sizeof(s_txBuf));
  size_t loaded = 0;
  i2s_channel_preload_data(s_txChan, s_txBuf, sizeof(s_txBuf), &loaded);

  err = i2s_channel_enable(s_txChan);
  if (err == ESP_OK)
    err = i2s_channel_enable(s_rxChan);
  if (err != ESP_OK) {
    Serial.printf("[Audio] i2s_channel_enable failed: %d\r\n", err);
    return false;
  }

  Serial.printf("[Audio] I2S duplex started: %d Hz, BCLK=%d WS=%d DOUT=%d DIN=%d (no MCLK)\r\n",
                AUDIO_SAMPLE_RATE, bclk, ws, dout, din);
  return true;
}

bool AudioCodec::writeFrame(const int16_t *in) {
  for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
    s_txBuf[i * 2] = (int32_t)in[i] << 16;
    s_txBuf[i * 2 + 1] = (int32_t)in[i] << 16;
  }
  size_t written = 0;
  esp_err_t err = i2s_channel_write(s_txChan, s_txBuf, sizeof(s_txBuf), &written, portMAX_DELAY);
  return err == ESP_OK && written == sizeof(s_txBuf);
}

bool AudioCodec::readSamples(int16_t *out, int n, uint64_t *startIdx) {
  if (!s_reader)
    s_reader = xTaskGetCurrentTaskHandle();
  for (;;) {
    uint64_t w;
    portENTER_CRITICAL(&s_dmaMux);
    w = s_dmaCount;
    portEXIT_CRITICAL(&s_dmaMux);
    if (w - s_readIdx > (uint64_t)(RX_RING - 2 * n)) {
      s_readIdx = w - n; // reader fell behind the ring - skip to the newest
      s_rxOverruns = s_rxOverruns + 1;
    }
    if (w - s_readIdx >= (uint64_t)n)
      break;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0)
      return false; // no audio for 100 ms
  }
  for (int i = 0; i < n; i++)
    out[i] = s_ring[(s_readIdx + i) & (RX_RING - 1)];
  *startIdx = s_readIdx;
  s_readIdx += n;
  return true;
}
