#ifndef ES8311_H
#define ES8311_H

#include <Arduino.h>
#include <Wire.h>

// Minimal ES8311 driver: ADC + DAC, I2S slave, 8 kHz, 16-bit in 32-bit slots.
// No MCLK pin - the codec's internal clock comes from SCLK (BCLK = 64 fs =
// 512 kHz, multiplied x4 to 2.048 MHz = 256 fs). Register sequence follows
// Espressif's esp_codec_dev ES8311 driver.
class ES8311 {
public:
  // I2S (BCLK/LRCK) must already be running before calling this.
  bool begin(TwoWire *wire, uint8_t addr, uint8_t pgaGain);

  // ADC PGA gain step 0-10 = 0-30 dB (3 dB steps)
  // Returns true when REG14 reads back the written value
  bool setPgaGain(uint8_t step);

  uint16_t chipId() const { return _chipId; }

  // Serial diagnostic: print registers 0x00-0x45, write one register
  void dumpRegs();
  bool writeReg(uint8_t reg, uint8_t val) { return _write(reg, val); }

private:
  TwoWire *_wire = nullptr;
  uint8_t _addr = 0;
  uint16_t _chipId = 0;

  bool _write(uint8_t reg, uint8_t val);
  int _read(uint8_t reg);
};

#endif
