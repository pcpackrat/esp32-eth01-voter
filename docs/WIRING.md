# ESP32-ETH01-Voter - Wiring

WT32-ETH01 + ES8311 codec module + GPS + Motorola CDM (16-pin rear accessory connector). Full duplex: receive and transmit.

## 1. Block diagram

```
                          CDM 1250 16-pin accessory
  ┌────────────┬──────────────┬─────────────┬──────────┬───────────┬──────────┬─────────┐
  │ SW B+ (13) │ RX audio (11)│ TX audio (5)│ PTT (3)  │ COS (12)  │ RSSI (15)│ GND (7) │
  ▼            ▼              ▲             ▲          ▼           ▼          ▼
┌──────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐ ┌─────────┐ ┌────────┐   │
│ Fuse/TVS │ │ AC couple │ │ AC couple │ │ 2N7002  │ │ 1k      │ │Divider │   │
│ Rev diode│ │ + divider │ │ + pad     │ │ (open   │ │ series  │ │ + RC   │   │
│ TSR      │ │           │ │           │ │  drain) │ │         │ │        │   │
└────┬─────┘ └─────┬─────┘ └─────▲─────┘ └────▲────┘ └────┬────┘ └───┬────┘   │
     │ 5V          │ MIC1P/N     │ OUTP       │ IO32      │ IO17     │ IO39   │
     │             ▼             │            │           │          │        │
     │       ┌──────────────────────┐         │           │          │        │
     │       │  ES8311 module       │ I2C: IO15 SDA, IO14 SCL        │        │
     │       │  ADC + DAC           │ I2S: IO2 BCLK, IO33 LRCK,      │        │
     │       │                      │      IO36 DIN, IO12 DOUT       │        │
     │       └──────────┬───────────┘         │           │          │        │
     ▼                  ▼                     │           ▼          ▼        ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                             WT32-ETH01                             RJ45 ─────┼──► LAN
│  5V  3V3  IO15 IO14 IO2 IO33 IO36 IO12  IO32  IO17  IO39  IO5  IO35  IO4  GND │
└──────┬─────────────────────────────────────────────────────┬─────▲─────▲───┬─┘
       │ 3V3                                                 │ RX  │ TX  │PPS│
       │                                             ┌───────▼─────┴─────┴───┴┐
       └───────────────────────────────────────────► │ +3.3V LEA-5T board Gnd │
                                                     └────────────────────────┘
```

## 2. WT32-ETH01 pin table

| WT32 pin | Direction | Signal | Connects to |
|---|---|---|---|
| 5V | in | Power | TSR 1-2450 output (section 6). Also feeds the GPS antenna (V ant, section 8) |
| GND | — | Ground | Common ground (section 7) |
| 3V3 | out | 3.3 V | ES8311 module (4.7), GPS board +3.3V (section 8), I2C pull-ups |
| IO15 | I/O | I2C SDA | ES8311 SDA, 4.7k pull-up to 3V3 |
| IO14 | out | I2C SCL | ES8311 SCL, 4.7k pull-up to 3V3 |
| IO2 | out | I2S BCLK | ES8311 SCLK |
| IO33 (header 485_EN) | out | I2S WS | ES8311 LRCK |
| IO36 | in | I2S data in | ES8311 ASDOUT |
| IO12 | out | I2S data out | ES8311 DSDIN |
| IO32 (header CFG) | out | PTT | PTT driver (4.6) |
| IO17 (header TXD) | in | COS | CDM pin 12 through 1k series (4.3) |
| IO39 | in (ADC1_CH3) | RSSI voltage | RSSI input network (4.4) |
| IO35 | in | GPS NMEA | GPS TXD (section 8) |
| IO4 | in | GPS PPS | GPS PPS / 1PPS (section 8) |
| IO5 (header RXD) | out | GPS config | GPS RXD, optional, for timing mode (section 8) |
| IO1 / IO3 / IO0 | — | Flashing | USB-serial (section 3) |

Do not use IO0, 16, 18, 19, 21, 22, 23, 25, 26, 27. They belong to the LAN8720 Ethernet PHY. Every other header pin is in use.

**Onboard LEDs on IO5 and IO17.** The header RXD (IO5) and TXD (IO17) pins each drive an activity LED (LED3 / LED4) that lights when the pin is low. LED4 therefore lights while COS is active, and LED3 flickers when the firmware talks to the GPS.

**Strap pins in use:**
- **IO12 (I2S DOUT)** must be low at boot. High selects 1.8 V flash and the board will not boot. It drives the codec's DSDIN input, which has no pull-up, so it stays low. If the codec module has a pull-up on DSDIN, remove it or fit 10k from IO12 to GND. (LRCK is on IO33 rather than IO12 because codec modules often pull LRCK up.)
- **IO2 (BCLK)** must be low or floating to flash. Do not pull it up.
- **IO15 (SDA)** high at boot is normal; the 4.7k pull-up is fine.

- **IO5 (GPS config)** outputs a PWM signal during boot. Harmless on a GPS RXD line.

**PTT is on IO32** because it is not a strap pin and stays quiet during boot, so the radio can't key while the board powers up. The firmware drives it to the unkeyed level first thing in setup.

**RSSI must be on an ADC1 pin.** ADC2 pins (IO2, IO4, IO12, IO14, IO15) stop working while WiFi runs. The ADC1 pins on this header are IO32, IO33, IO35, IO36 and IO39.

## 3. Flashing header

```
USB-serial (3.3 V)        WT32-ETH01
  TX  ─────────────────►  RX0 (IO3)
  RX  ◄─────────────────  TX0 (IO1)
  GND ──────────────────  GND
                          IO0 ──[jumper]── GND   (fit for flashing, remove to run)
```
The WT32 has no auto-reset. Fit the jumper, power-cycle or reset, flash, remove the jumper, then reset. After the first flash, the Firmware web page can update over the network.

## 4. Radio interface

### 4.1 ES8311 connections

| ES8311 module pin | WT32 pin | Notes |
|---|---|---|
| SDA | IO15 | 4.7k pull-up to 3V3 (skip if the module has one) |
| SCL | IO14 | 4.7k pull-up to 3V3 (skip if the module has one) |
| SCLK (BCLK) | IO2 | No pull-up |
| LRCK (WS) | IO33 (header 485_EN) | Not a strap pin - a module pull-up here is fine |
| ASDOUT (DOUT) | IO36 | Codec ADC data to the ESP32 |
| DSDIN (DIN) | IO12 | ESP32 TX audio to the codec DAC. No pull-up (strap note, section 2) |
| OUTP / OUTN | — | DAC output, to the radio TX audio (4.5) |
| MCLK | — | Leave open, or tie to GND if it floats |
| CE / AD0 | GND | I2C address 0x18 |
| VCC / GND | 3V3 / GND | Section 4.7 |

- **No MCLK.** The ESP32 can only output MCLK on IO0, IO1 or IO3. IO0 is the Ethernet clock and IO1/IO3 are the console. The ES8311 takes its clock from BCLK instead.
- Keep the I2S wires short (under ~10 cm). BCLK is only 256 kHz, but a long BCLK wire still couples into the audio.
- **Check:** the boot log shows `[ES8311] Init OK (ID 0x8311)`, and the status page shows Codec OK. If the codec is found but "ADC peak" shows no level with audio applied, flip `AUDIO_RX_SLOT` in include/AudioCodec.h and rebuild.

### 4.2 RX audio → ES8311 MIC1P / MIC1N

```
CDM 11 ──C1 10µF NP──R1 10k──┬─────────┬──────C2 1µF──── MIC1P
                             │         │
                            R2 10k    C4 1nF
                             │         │
CDM 7 (audio return) ────────┴─────────┴──────C3 1µF──── MIC1N
```

| Part | Value | Purpose |
|---|---|---|
| C1 | 10 µF non-polar, ≥16 V | Blocks radio DC |
| R1 / R2 | 10k / 10k | Level divider, -6 dB to start. Smaller R2 = quieter |
| C4 | 1 nF | RF and hiss filter (~32 kHz corner with R1‖R2) |
| C2 / C3 | 1 µF film or X7R | Codec input coupling. The ES8311 biases MIC1P/N internally. Omit these if the module already has coupling caps on MIC1P/N |

- MIC1N goes to the radio's audio return, not to local GND. The codec input is differential, so ground noise between the radio and the WT32 is rejected.
- Run CDM 11 and the audio return as a twisted pair or shielded cable. Ground the shield at the radio end only.
- If the module has an onboard mic, remove it or cut its traces to MIC1P/N. Firmware keeps the mic bias off.
- **Level:** full-deviation voice should give "ADC peak" about -6 to -10 dBFS on the status page. Adjust R2 first, then the codec PGA (Radio page, 0-30 dB), then Digital Gain. Trim Digital Gain for "Encoder peak" -3 to -6 dB.

### 4.3 COS → IO17

Direct connection. The CDM COS output is active low.

```
CDM pin 12 ──R7 1k── IO17     (firmware enables the internal ~45k pull-up)
CDM pin 7  ───────── GND
```

- R7 (1k) protects IO17 in case the radio drives the line while the pin is misconfigured.
- It doesn't affect reading. With the radio pulling low, IO17 sits at about 0.07 V (3.3 V × 1k / 46k), well below the ~0.8 V LOW threshold.
- Radio COS **low** = signal present → firmware sees "active" with Invert COS **off** (default).
- The onboard LED4 is wired to this pin and lights while COS is active. Its path to 3V3 also helps the pull-up.
- **Check:** with the radio unplugged, IO17 reads about 3.3 V. On the status page, COS shows OPEN (and LED4 lights) only while a signal is present.

### 4.4 Hardware RSSI → IO39 (optional)

CDM 1250 accessory **pin 15** = RSSI. Measure its voltage range (no signal and full quieting) before fitting the divider. If it never exceeds 2.4 V, use R12 = 1k and leave R13 out. With "Use Hardware RSSI" off, the DSP noise RSSI is used instead.

```
CDM pin 15  ──R12 10k──┬──────────┬──────── IO39 (ADC1_CH3)
                       │          │
                      R13 8.2k   C5 100nF
                       │          │
GND ───────────────────┴──────────┴──────── GND
```

| Part | Value | Notes |
|---|---|---|
| R12 / R13 | 10k / 8.2k | 5 V max RSSI → 2.25 V at the pin. ADC range is about 0.15-2.45 V. For RSSI ≤ 2.4 V, fit R12 = 1k and leave R13 out |
| C5 | 100 nF | Smooths the RSSI voltage (~0.5 ms). The firmware also averages each 20 ms frame |

**Calibrate:** Radio page → Run RSSI calibration. Step 1 with no signal, step 2 with a full-quieting carrier. One run calibrates both the RSSI pin and the DSP noise RSSI; choose which one to use and save. Radio set to Flat Unsquelched audio and the PGA at its final setting first.

### 4.5 TX audio: ES8311 OUTP → CDM pin 5

```
ES8311 OUTP ──C10 10µF NP──R10 10k──┬──────────── CDM 5 (TX audio in)
                                    │
                                   R11 1k
                                    │
ES8311 GND ─────────────────────────┴──────────── CDM 7 (GND)

ES8311 OUTN ── leave open
```

| Part | Value | Purpose |
|---|---|---|
| C10 | 10 µF non-polar, ≥16 V | Blocks the codec's output bias (about half the supply) and radio DC |
| R10 / R11 | 10k / 1k | Level pad, about -21 dB to start. Larger R11 = louder |

- OUTP/OUTN are a differential pair biased at mid-supply. Taking OUTP alone, through C10, gives half the swing, which is plenty for a radio line input.
- **Take the signal at the codec pins, before any speaker amplifier on the module.** Many ES8311 modules have an amp (e.g. NS4150) after OUTP/OUTN. Its output is bridged and must not be grounded on one side. Remove the speaker, and disable or lift the amp if it loads the outputs.
- Run CDM 5 and ground as a twisted pair or shielded cable, shield grounded at the radio end only.
- **Level:** set a monitor receiver or service monitor on the transmitter. Send full-level audio from the host (a test tone through the node). Pick R11 so TX Gain (Radio page) near 100 % gives normal deviation, then trim with TX Gain. The status page shows "DAC peak" while host audio plays.

### 4.6 PTT: IO32 → CDM pin 3

The CDM PTT input is active low with an internal pull-up. An N-channel MOSFET pulls it to ground when IO32 is high.

```
IO32 ──R20 100Ω──┬──────── Q1 gate          Q1 = 2N7002 (or BSS138)
                 │
                R21 100k                    Q1 drain ──── CDM 3 (PTT)
                 │                          Q1 source ─── GND
GND ─────────────┴──────────────────────────────────────── CDM 7
```

| Part | Value | Purpose |
|---|---|---|
| Q1 | 2N7002 / BSS138 | Logic-level N-MOSFET, open drain to the radio's PTT line |
| R20 | 100 Ω | Gate series resistor |
| R21 | 100k | Holds the gate low while IO32 is not driven (reset, boot, flashing) - the radio can't key |

- IO32 **high** = keyed with PTT Invert **off** (default). The firmware sets IO32 low first thing at boot.
- A 2N3904 works too: 4.7k from IO32 to the base, 100k base to GND, emitter to GND, collector to CDM 3.
- PTT stays keyed while host audio arrives, plus the PTT Tail (Radio page, default 500 ms).
- **Check:** the serial log shows `[PTT] KEYED` / `[PTT] released`, and the status page Transmitter card shows PTT KEYED while the host sends audio.

### 4.7 Codec power

```
WT32 3V3 ──FB2──┬────────┬──────── ES8311 module VCC (AVDD/DVDD)
                │        │
              C5 10µF  C6 100nF
                │        │
GND ────────────┴────────┴──────── ES8311 module GND
```

- FB2 (600 Ω @ 100 MHz) keeps WT32 digital noise off the codec analog supply. The ES8311 draws about 10-20 mA.
- Bring the module GND to the single ground point (section 7), right next to the WT32 GND pin.

### 4.8 CDM 1250 accessory connector

| CDM pin | Name | Goes to |
|---|---|---|
| 3 | DIG_IN_1, programmed in CPS as **Data PTT** (PTT that takes audio from pin 5) | 4.6 PTT (Q1 drain) |
| 5 | TX audio input (flat) | 4.5 TX audio (R10/R11) |
| 7 | GROUND | System ground, the RX audio return (MIC1N side) and the TX audio ground |
| 11 | FLAT/FILTERED_RX_AUDIO, set to **Flat Unsquelched** in CPS | 4.2 input (C1) |
| 12 | DIG_IN_OUT_7, programmed in CPS as channel activity / carrier detect | 4.3 COS input (R7) |
| 13 | SWB+ (follows radio power switch) | Power input, section 6 |
| 15 | RSSI | 4.4 RSSI input (R12) |

- Pin 3 must be a PTT type that switches the TX audio source to pin 5 (named Data PTT or similar in the CPS Accessories tab). A plain external-mic PTT keys the radio but takes audio from the mic pin (2). Check the names against your CPS version.
- Pin 14 (DIG_IN_OUT_8) also works for COS. Avoid 6 and 8, which double as Sel5/MDC/MPT UART.

Not used: 1/16 speaker, 2 mic, 4 alarm, 9 emergency, 10 ignition.

## 5. Header CFG pin

IO32 is the header pin labelled CFG. In the stock Wireless-Tag AT firmware it is a factory-reset input; this firmware uses it only as the PTT output.

## 6. Power from CDM switched B+

Regulator: **TRACO TSR 1-2450** (5 V, 1 A, ~6.5-36 V in, 3-pin SIP: 1 = +Vin, 2 = GND, 3 = +Vout).

```
CDM 13 SWB+  ──F1──D1──►──┬──────────┬──────────┬──── pin 1    ┌────────────┐
 (13.8 V)      PTC  SS34  │          │          │              │ TSR 1-2450 │
              500mA      TVS1      C1 10µF    C2 100nF         │            │
                         SMBJ18A   50V cer.     │      ┌─pin 2 │            │
                          │          │          │      │       └─────┬──────┘
CDM 7 GND ────────────────┴──────────┴──────────┴──────┘             │ pin 3
                                                                     │
                          ┌──────────────────────────────────────────┘
                          │
                          ├── C3 22µF ──┬── C4 100nF ──FB1──► WT32 5V
                          │             │                     (onboard AMS1117 makes 3V3)
GND (pin 2) ──────────────┴─────────────┴──────────────────► WT32 GND
```

| Part | Value | Notes |
|---|---|---|
| F1 | PTC 500 mA hold | Protects the radio's switched B+ line |
| D1 | SS34 Schottky, series | Reverse-polarity protection (~0.4 V drop) |
| TVS1 | SMBJ18A | 18 V standoff, clamps ~29 V. Below the TSR's 36 V input maximum |
| C1 / C2 | 10 µF 50 V ceramic / 100 nF | TSR input, right at pin 1-2 |
| C3 / C4 | 22 µF 10 V / 100 nF | TSR output, right at pin 3-2 |
| FB1 | Ferrite bead, 600 Ω @ 100 MHz | Keeps switching noise off the WT32 rail |

The TSR runs without external caps. C1-C4 are there to keep its switching noise off the radio cable and the WT32 rail. Check the TRACO datasheet for the recommended values.

**Load budget at 5 V:**

| Load | Current |
|---|---|
| WT32-ETH01 (Ethernet) | ~150-200 mA |
| WT32-ETH01 (WiFi active) | ~300 mA peaks |
| GPS (LEA-5T + active antenna) | ~60-75 mA |
| ES8311 module | ~10-20 mA |
| **Total** | **~270 mA typical, 400 mA peak** |

That is about 100-140 mA from 13.8 V through the TSR (about 90% efficient), well inside a CDM switched B+ output. It also uses only a quarter to a third of the TSR's 1 A rating.

## 7. Grounding and layout

- One ground point: radio GND, TSR GND, WT32 GND, ES8311 GND and GPS GND meet at one spot near the WT32 GND pin.
- Keep the ES8311 module close to the WT32 (short I2S wires), its input network (4.2) right at MIC1P/N, and the TX pad (4.5) right at OUTP.
- Keep the TX audio and PTT wires away from the RX audio pair. Ground the TX audio return at CDM 7, not at the WT32.
- Route RX audio and its return as a twisted pair or shielded cable from the CDM connector. Ground the shield at the radio end only.
- Keep the RSSI divider and C5 close to IO39.
- Keep the TSR away from the codec, its input wiring and IO39.
- Keep the GPS antenna cable away from the TSR.

## 8. GPS

Module: u-blox **LEA-5T** timing receiver on the WD22UGRC carrier board (schematic: docs/GPS-Schematic.png), with an active antenna on the SMB connector. 9600 baud NMEA, 8N1.

The board has no regulator: its +3.3V pin feeds the LEA-5T VCC directly, and TxD, RxD and PPS are 3.3 V logic through 33 Ω series resistors and an ESD array. It connects straight to the WT32 with no level shifting.

```
WT32-ETH01                        WD22UGRC 8-pin header
  3V3  ──────────┬──────┬───────►  +3.3V
                 │      │
               C6 10µF C7 100nF
                 │      │
  GND  ──────────┴──────┴────────  Gnd
  5V   ─────────────────────────►  V ant   (5 V active antenna supply)
  IO35 ◄─────────────────────────  TxD
  IO4  ◄─────────────────────────  PPS
  IO5  ─────────────────────────►  RxD     (timing mode setup)
```

| Board pin | WT32 pin | Notes |
|---|---|---|
| +3.3V | 3V3 | LEA-5T supply, from the WT32 onboard AMS1117. **3.3 V only** - there is no regulator on the board |
| Gnd | GND | Run it with the +3.3V wire, back to the single ground point (section 7) |
| V ant | 5V | Feeds the 5 V active antenna through the board's current limiter, up the SMB coax. Take it from the WT32 5V pin (TSR output, after FB1). Only V ant gets 5 V - +3.3V stays on 3V3 |
| TxD | IO35 | NMEA out. IO35 is input-only, no pull-up needed |
| PPS | IO4 | LEA-5T TIMEPULSE, 1 pulse/s, rising edge. Firmware enables the internal pull-down |
| RxD | IO5 | Lets the firmware set up timing mode (GPS web page). Without it the GPS still works, but timing mode can't be turned on |

| Part | Value | Notes |
|---|---|---|
| C6 / C7 | 10 µF / 100 nF | At the board's +3.3V pin. Covers the acquisition current peaks |

- Current: about 50 mA for the LEA-5T from 3V3, plus 10-25 mA for the antenna from 5V.
- Don't swap V ant and +3.3V. 5 V on +3.3V goes straight into the LEA-5T VCC and will damage it.
- Keep the TxD/PPS/RxD wires short (under ~30 cm) and away from the TSR.
- Keep the antenna cable away from the TSR.
- **Timing mode** (GPS web page): the LEA-5T surveys its position once, saves it, then keeps time on as little as 1 satellite. Use it at a fixed site with a poor sky view. The status page shows survey progress and the fixed position.
- **Check:** on the status page, NMEA data shows good sentences, PPS shows OK (1 Hz), GPS = LOCKED, and the audio clock locks to GPS about 10-20 s later. With RxD wired, the Timing mode line shows the module answered (Off / Survey-in / Fixed).
