# Remote Sensor Node - XIAO nRF52840 + Wio-SX1262

Battery-powered LoRa P2P temperature node. Reads a DS18B20, transmits a 6-byte packet
every few minutes, and deep-sleeps in between. Transmit-only - it never listens.

Pairs with the hub in [../lora-remote-temperature.yaml](../lora-remote-temperature.yaml),
which republishes readings to Home Assistant.

```
  [XIAO nRF52840 + Wio-SX1262]  --LoRa 868 MHz-->  [Heltec V2 hub / ESPHome]  --WiFi/API-->  Home Assistant
   (this firmware, sleeps)          raw 6-byte pkt     (sx127x, continuous RX)
```

## Hardware

- **Seeed XIAO nRF52840 & Wio-SX1262 Kit for Meshtastic** (SKU 102010710). The Wio-SX1262
  (Semtech **SX1262** + TCXO + RF switch + antenna) stacks on the XIAO via the kit header.
- **DS18B20** 1-Wire temperature sensor (TO-92), externally powered (not parasitic).
- LiPo on the XIAO's `BAT+ / BAT-` pads (onboard charger).

Cross-chip is fine: SX1262 (node) <-> SX1276 (hub) interoperate at the PHY level as long as
every modulation parameter matches (see below).

### Pin map

Radio pins are made by the stacked kit header (nothing to hand-wire); the sketch just needs
them for RadioLib. Values taken from Meshtastic's `seeed_xiao_nrf52840_kit/variant.h`
(the kit builds `SEEED_XIAO_NRF_KIT_DEFAULT`, i.e. the non-BTB, non-legacy block).

| Function             | XIAO pin              | Notes |
|----------------------|-----------------------|-------|
| SX1262 NSS/CS        | D4                    | `Module(CS, DIO1, RST, BUSY)` |
| SX1262 DIO1          | D1                    | IRQ |
| SX1262 RESET         | D2                    | |
| SX1262 BUSY          | D3                    | |
| SX1262 RXEN          | D5                    | RF-switch RX enable; held low (TX-only) |
| SPI SCK/MISO/MOSI    | D8 / D9 / D10         | hardware SPI (by header) |
| DS18B20 data         | D0                    | 4.7k pull-up to the **switched** rail (D6), not 3V3 |
| DS18B20 power (gate) | D6                    | high = on, low = fully de-energised |
| VBAT read            | P0.31 (`32`)          | internal; onboard R17=1M / R18=510k divider |
| VBAT divider enable  | P0.14 (`14`)          | drive **LOW** to read, HIGH to disable |

Free/unused: D7.

**Two Wio-SX1262 must-haves** (either wrong = the module never transmits, no error):
- **TCXO 1.8 V on DIO3** - `radio.begin(..., 1.8, ...)`. The module has a TCXO, not a crystal.
- **DIO2 gates the TX RF switch** - `radio.setDio2AsRfSwitch(true)`.

### Connection diagram

Only the hand-wired parts are shown. The radio (D1-D5, D8-D10, 3V3, GND) is made by the
stacked kit header and is omitted.

```
   Seeed XIAO nRF52840                         DS18B20 (TO-92)
   +--------------+                            flat face toward you, pins down:
   |              |                            +---------------+
   |  D6  --------+----------+-----------------+ VDD  (right)  |
   |  (power)     |          |                 |               |
   |              |        [4.7k]              |               |
   |              |          |                 |               |
   |  D0  --------+----------+-----------------+ DQ   (middle) |
   |  (1-wire)    |                            |               |
   |              |                            |               |
   |  GND --------+----------------------------+ GND  (left)   |
   |              |                            +---------------+
   |  BAT+ <-- LiPo +          4.7k pull-up ties DQ to D6 (the SWITCHED rail),
   |  BAT- <-- LiPo -          NOT 3V3 -> D6 low in sleep = 0 V bus, ~0 drain.
   +--------------+
   Battery sense is internal (P0.31 via R17/R18 divider, P0.14 enable) - no external wiring.
```

**Multiple sensors on one pin:** wire extra DS18B20s in parallel on the same three rails
(D6 / D0 / GND) - keep the single 4.7k pull-up. The 1-Wire protocol addresses 100+ devices
(each has a unique 64-bit ROM), so the data line is not the limit here.

The limit is **powering from D6** (a GPIO): `requestTemperatures()` converts all sensors at
once, ~1.5 mA each, drawn from D6. At the nRF52840's default drive strength that budget runs
out around **3-4 sensors** (VDD must stay >=3.0 V; the 4.7k pull-up adds ~0.7 mA). To go
higher: enable high-drive on D6 (`PIN_CNF`, not exposed by `pinMode`), power the string from
3V3 instead of the gated GPIO, or convert one address at a time so currents don't stack.

> The sketch reads only `getTempCByIndex(0)`. More than one sensor also needs firmware
> changes: read each by index/address and widen the packet (or send one packet per sensor).

> Never transmit without the antenna attached - it can damage the SX1262 PA.

## Radio parameters (identical on node and hub)

| Parameter        | Value          |
|------------------|----------------|
| Band             | EU868          |
| Frequency        | 868.1 MHz      |
| Modulation       | LoRa           |
| Spreading factor | SF10           |
| Bandwidth        | 125 kHz        |
| Coding rate      | 4/5            |
| Sync word        | 0x12 (private) |
| Preamble         | 8 symbols      |
| Header           | explicit       |
| CRC              | on             |

`0x12` == `RADIOLIB_SX126X_SYNC_WORD_PRIVATE`, so it maps straight to the hub's
`sync_value: 0x12` - no SX126x/SX127x sync-word translation needed on top.

## Packet format

Fixed **6-byte, little-endian** payload. No app checksum - the LoRa PHY CRC covers integrity.
This must match the hub's decoder byte-for-byte; the encoder lives in
[packet.h](packet.h).

| Offset | Field   | Type  | Encoding                                   |
|--------|---------|-------|--------------------------------------------|
| 0      | node_id | u8    | 1..255, unique per node                    |
| 1..2   | temp    | int16 | centi-degrees C, signed (`round(C * 100)`) |
| 3..4   | batt_mv | u16   | battery millivolts                         |
| 5      | seq     | u8    | send counter, wraps 255 -> 0               |

Worked examples (TX byte order), same as the hub README:

- node 1, `21.37 C`, `3.912 V`, seq 42 -> `01 59 08 48 0F 2A`
- node 2, `-5.20 C`, `4.010 V`, seq 7  -> `02 F8 FD AA 0F 07`

## Power notes

- **1-Wire**: the DS18B20 (and its 4.7k pull-up, tied to D6) is powered only during a read;
  D6 goes low afterwards and the data pin is set to input, so the bus sits at 0 V in sleep.
  Normal (externally-powered) mode, never parasitic, so it can be fully cut.
- **Battery ADC**: the divider is enabled (P0.14 LOW) only around the `analogRead`, then
  disabled (P0.14 HIGH) - enable-before / disable-after.
- **Radio**: `radio.sleep()` before the MCU sleeps.
- **MCU sleep**: `delay()` on the Adafruit nRF52 core enters FreeRTOS tickless idle, which
  keeps RAM (so `seq` survives). For absolute-minimum current, System OFF
  (`sd_power_system_off`) is lower but loses RAM - store `seq` in `NRF_POWER->GPREGRET`. The
  hub tolerates `seq` resets (counts as one dropped interval), so tickless idle is the default.

## Build & flash

1. Arduino IDE -> Boards Manager -> install **Seeed nRF52 Boards** (Adafruit nRF52 core).
   Select **Seeed XIAO nRF52840**.
2. Library Manager -> install **RadioLib**, **OneWire**, **DallasTemperature**.
3. Set `NODE_ID` (and `SEND_INTERVAL_MS`) at the top of
   [remote-sensors.ino](remote-sensors.ino).
4. Upload. If it won't enter the bootloader, double-tap RESET to expose the UF2 drive.

Adding another node: give it a unique `NODE_ID`, then mirror the hub's `node2_*` blocks -
see "Adding another node" in [../README.md](../README.md).

## Verify the packet encoder off-device

`packet.h` is pure C++, so the wire format can be checked on the host without hardware:

```bash
c++ -x c++ -std=c++17 -o /tmp/pkttest - <<'EOF'
#include <cstdio>
#include <cstring>
#include "packet.h"
int main() {
  uint8_t b[packet::SIZE];
  packet::build(1, 2137, 3912, 42, b);            // 21.37 C, 3.912 V, seq 42
  const uint8_t want[] = {0x01,0x59,0x08,0x48,0x0F,0x2A};
  packet::build(2, -520, 4010, 7, b);             // -5.20 C -> temp bytes F8 FD
  return 0;
}
EOF
```

(The sketch's own `loop()` also logs `sent node=.. temp=.. batt_mv=.. seq=..` over USB serial.)
On the hub side, watch the ESPHome logs for the matching `node=1 temp=.. rssi=.. snr=..` line.
