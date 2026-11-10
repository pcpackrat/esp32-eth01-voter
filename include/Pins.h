#ifndef PINS_H
#define PINS_H

// WT32-ETH01 header pin assignments (docs/WIRING.md, ES8311 codec).
// Pins reserved by the LAN8720 PHY: 0, 16, 18, 19, 21, 22, 23, 25, 26, 27.
// UART0 (IO1/IO3) is the console and flashing port.
// IO5 (header RXD) and IO17 (header TXD) each drive an onboard activity LED
// (LED3 / LED4, lit when the pin is low).

// --- ES8311 codec ---
// No MCLK: the ESP32 can only output it on IO0/1/3 (Ethernet clock / console).
// The codec takes its clock from BCLK instead.
#define PIN_I2C_SDA 15  // 4.7k pull-up. Strap pin, high at boot is normal
#define PIN_I2C_SCL 14  // 4.7k pull-up
#define PIN_I2S_BCLK 2  // -> ES8311 SCLK. Strap pin: no pull-up
#define PIN_I2S_WS 33   // -> ES8311 LRCK. Header 485_EN. Not a strap pin
#define PIN_I2S_DIN 36  // <- ES8311 ASDOUT. Input-only pin
#define PIN_I2S_DOUT 12 // -> ES8311 DSDIN. Strap pin: must be low at boot
                        //    (codec input has no pull-up, DOUT idles low)
#define ES8311_I2C_ADDR 0x18 // CE/AD0 low

// --- GPS ---
#define PIN_GPS_RX 35 // GPS TX -> ESP32. Input-only pin.
// ESP32 -> GPS RXD (header RXD pin), needed only for GPS timing mode (UBX
// setup). Harmless if not wired. Strap pin, outputs PWM during boot.
#define PIN_GPS_TX 5
#define PIN_GPS_PPS 4 // Interrupt input
#define GPS_BAUD 9600

// --- Radio ---
// COS direct from the radio (active low) through a BAT85 series diode, internal pull-up.
// Header TXD pin. LED4 lights while COS is active.
#define PIN_COS 17
// Hardware RSSI voltage. ADC1 (ADC2 pins don't work while WiFi runs).
// 0-2.45 V at the pin - divide down higher RSSI voltages.
#define PIN_RSSI_ADC 39 // ADC1_CH3 (SENSOR_VN). Input-only

// PTT to the radio through a transistor (high = keyed with PTT Invert off).
// Header CFG. Not a strap pin and quiet at boot, so the radio can't key
// during power-up.
#define PIN_PTT 32

// --- LAN8720 PHY (fixed by the WT32-ETH01) ---
#define WT32_ETH_PHY_ADDR 1
#define WT32_ETH_PHY_POWER 16
#define WT32_ETH_PHY_MDC 23
#define WT32_ETH_PHY_MDIO 18

#endif
