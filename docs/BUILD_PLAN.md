# ESP32-ETH01-Voter - Build Plan

Full duplex voter client on a **WT32-ETH01 + ES8311 codec module**, for a Motorola CDM radio. Compatible with Asterisk chan_voter and the original PIC VOTER boards. Can be a chan_voter client or the master.

Derived from ESP32RXVoter (receive only) with the transmit path of the ESP32-P4 voter. Pinout and parts: [WIRING.md](WIRING.md).

## 1. What's in the firmware

| Area | Notes |
|---|---|
| Voter protocol | Auth, GPS packets, audio packets, host option flags (master / flat audio / no PL filter) |
| GPS-locked audio timebase | PLL on PPS, resampler: frames on the exact GPS 20 ms grid, stamped with GPS time (same convention as the PIC). Built-in offsets: client -100 ms, master -120 ms (picked by the host master flag), plus a trim |
| Audio | ES8311 over I2S, 8 kHz 16-bit, 1 ms DMA blocks. Codec clocked from BCLK (no MCLK pin available) |
| RX DSP | Noise RSSI, PL filter (290 Hz high-pass), de-emphasis, digital gain, uLaw |
| RSSI | DSP noise or hardware (IO39), both calibrated from the web page |
| Squelch | COS input, DSP squelch (open level + hysteresis) or always on, 200 ms tail frames |
| TX | Host audio -> jitter buffer -> uLaw decode -> TX gain -> DAC. PTT while host audio arrives, plus tail |
| GPS | NMEA + PPS, LEA-5T timing mode (survey-in then fixed position), UTC/leap-second check |
| Network | Ethernet / WiFi / Auto, setup hotspot, PHY flap recovery |
| Web UI | Status, Voter, Radio, Network, WiFi, GPS, RSSI calibration, firmware update (OTA) |
| Syslog, serial console | `status`, `reboot`, `reset` (factory defaults, asks first) |

### Task layout
```
core 1  rxTask   (prio 10)  codec RX (1 ms) -> GPS timebase -> DSP -> uLaw -> COS -> frame queue
core 1  txTask   (prio 10)  jitter buffer -> uLaw decode -> gain -> codec TX (paced by the DMA)
core 1  netTask  (prio 9)   frame queue -> voter packets
core 1  loop     (prio 1)   GPS / PPS -> PLL, network, voter protocol, PTT, web, syslog, serial
core 0  WiFi / ETH / lwIP   (IDF)
```

## 2. Parts list

| Qty | Part | Notes |
|---|---|---|
| 1 | WT32-ETH01 | Board with the CFG and 485_EN header pins (IO32 / IO33) |
| 1 | ES8311 codec module | I2C + I2S, MIC1P/N input, OUTP/OUTN output |
| 1 | u-blox LEA-5T on WD22UGRC board + 5 V active antenna | |
| 1 | TRACO TSR 1-2450 | 5 V switching regulator |
| 1 | 2N7002 or BSS138 | PTT driver |
| 1 | PTC fuse 500 mA, SS34 Schottky, SMBJ18A TVS | Input protection |
| - | Resistors, capacitors, ferrite beads | See WIRING.md |
| 1 | 3.3 V USB-serial adapter + jumper | First flash (IO0 to GND). Later updates over the web |
| 1 | CDM 16-pin accessory plug | |

## 3. Build steps

### Step 1 - Power and flashing
1. Build the power circuit (WIRING.md section 6). Check 5.0 V at the output before connecting the WT32.
2. Flash: USB-serial TX→RX0 (IO3), RX→TX0 (IO1), GND. IO0 to GND, power up or reset.
   ```
   pio run -t upload --upload-port COMx
   ```
   Remove the IO0 jumper and reset.
3. Serial console at 115200: banner, `[NET] Ethernet started`, `[ETH-EVT] GOT_IP`.
4. Browse to the IP. **Pass:** Status page loads, Network = ETH.

### Step 2 - Codec
1. Wire the ES8311 (WIRING.md 4.1, 4.7).
2. Boot log: `[Audio] I2S duplex started`, `[ES8311] Init OK (ID 0x8311)`. Status page: Codec OK.
3. Radio page: COS Mode = Always On. Feed a 1 kHz tone into the RX input network (4.2).
4. "ADC peak" tracks the tone. If there is no level at all, flip `AUDIO_RX_SLOT` in include/AudioCodec.h.
5. Frame counters: dropped, I2S errors and overruns stay at 0. **Pass:** clean level tracking.

### Step 3 - GPS
1. Wire the LEA-5T board (WIRING.md section 8), antenna with a sky view.
2. Status page: NMEA good sentences, PPS OK, GPS = LOCKED.
3. About 10-20 s later: Audio clock = LOCKED, PPS phase error a few µs. **Pass:** LOCKED and stable.
4. Optional: GPS page → Enable timing mode. The survey takes the minimum time set there (default 60 min), then the Timing mode line shows Fixed position.

### Step 4 - Voter server
1. Add the client to `voter.conf`. Set host, port and both passwords on the Voter page (reboots).
2. Voter state goes WAITING FOR GPS → WAITING FOR AUDIO CLOCK → CONNECTED.
3. With the tone and COS Always On, check the host receives the audio. In the chan_voter debug log the drain index should match the PIC clients (2080, or 1920 with a PIC master).

### Step 5 - CDM radio, receive
1. CPS: RX audio pin 11 **Flat Unsquelched**; pin 12 as channel activity / carrier detect (COS).
2. Wire COS (4.3), RX audio (4.2), RSSI (4.4).
3. Radio page: COS Mode = Hardware. COS shows OPEN only with a signal (LED4 lights too).
4. Level with full-deviation voice: R2 / PGA for ADC peak -6 to -10 dBFS, then Digital Gain for Encoder peak -3 to -6 dB.
5. Leave PL Filter and De-emphasis on (flat audio).
6. Run RSSI calibration (Radio page): no signal, then full-quieting carrier. Save with the RSSI source you want.

### Step 6 - CDM radio, transmit
1. CPS: pin 3 as **Data PTT** (PTT that takes audio from pin 5). Check the name against your CPS version.
2. Wire the TX pad (4.5) and the PTT driver (4.6). Into a dummy load or with a service monitor.
3. Send audio from the host (a test tone through the node). Serial: `[PTT] KEYED`. Status page Transmitter card: PTT KEYED, host frames counting up, DAC peak showing a level.
4. Set deviation: R11 for about the right level at TX Gain 100 %, then trim TX Gain.
5. Unkey: PTT drops after the PTT Tail (default 500 ms).
6. **Pass:** clean TX audio at normal deviation, no PTT at power-up or reset.

### Step 7 - Master test (optional)
1. Make this unit the master in voter.conf. Status: CONNECTED (master).
2. On a PIC transmitter's console, status 98: "Last Ntwk Rx Pkt Timestamp ... diff" about 20 ms and inbounds 1.

## 4. Open items / risks

| Item | Risk | Action |
|---|---|---|
| Codec clocked from BCLK (x8 inside the codec) | Not yet proven on hardware; the P4 uses an MCLK pin | Step 2 / 6 listening check. Fallback: MCLK on IO1 (gives up the serial console) |
| Codec module speaker amp | Some modules have an amp after OUTP/OUTN that loads or bridges the output | Take TX audio at the codec pins (WIRING.md 4.5) |
| IO12 strap pin (I2S DOUT) | A pull-up on DSDIN stops the board booting | Check the module; 10k to GND if needed |
| uLaw clip at 8159 | Inherited from TeensyVoter: clips at -12 dBFS of int16 | Use Digital Gain to set Encoder peak |

## 5. Default settings (factory reset: serial `reset`)

| Setting | Default |
|---|---|
| Timing trim | 0 ms (built-in offset -100 client / -120 master) |
| COS mode / invert | Hardware / off (active low at IO17) |
| RSSI source | DSP |
| DSP squelch open / hysteresis | 80 / 8 |
| PGA / digital gain | 0 dB / 100 % |
| PL filter / de-emphasis | on / on |
| TX gain / PTT invert / PTT tail / jitter buffer | 100 % / off (IO32 high = keyed) / 500 ms / 200 ms |
| Network | Auto (Ethernet when cabled, otherwise WiFi), DHCP |
| GPS timing mode | off (survey 60 min, 2 m) |
| Setup hotspot | `ETH01Voter_Config` at 10.5.5.5 when there is no network |

Host, port and passwords come from include/Site.h (not in git; copy Site.example.h).

Settings use the same storage as ESP32RXVoter, so flashing this over an RX unit keeps its settings and adds the TX defaults.
