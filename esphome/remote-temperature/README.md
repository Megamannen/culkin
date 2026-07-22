# LoRa Remote Temperature

Battery-powered outdoor temperature nodes that send readings over **LoRa P2P** (no WiFi at
the node) to a single mains-powered hub, which republishes them to Home Assistant.

```
  [XIAO nRF52840 + Wio-SX1262]  --LoRa 868 MHz-->  [Heltec V2 hub / ESPHome]  --WiFi/API-->  Home Assistant
   (Arduino/RadioLib, sleeps)      raw 6-byte pkt     (sx127x, continuous RX)
```

- Hub firmware: [lora-remote-temperature.yaml](lora-remote-temperature.yaml) (ESPHome). Done.
- Sensor firmware: **not in this repo** - build it against the contract below.

## Hardware

### Hub / receiver (this config)

- **Heltec WiFi LoRa 32 V2**: ESP32 + **SX1276** (Semtech), 0.96" OLED (unused), LiPo mgmt.
- Uses ESPHome's `sx127x` component. `board: heltec_wifi_lora_32_V2` does NOT wire the
  radio - the SPI + control pins are the fixed V2 PCB traces and are declared explicitly:

  | Signal | GPIO   | Notes                                   |
  |--------|--------|-----------------------------------------|
  | SCK    | GPIO5  | strapping pin (harmless warning at build) |
  | MOSI   | GPIO27 |                                         |
  | MISO   | GPIO19 |                                         |
  | NSS/CS | GPIO18 |                                         |
  | RST    | GPIO14 |                                         |
  | DIO0   | GPIO26 | RX-done interrupt                       |

  Confirm against your board revision before flashing.

### Sensor nodes (to build)

- **Seeed XIAO nRF52840** (Nordic nRF52840) + **Wio-SX1262** LoRa module (Semtech **SX1262**).
- Arduino (Adafruit nRF52 core) or Zephyr + **RadioLib** for the radio.
- Reads onboard battery voltage, deep-sleeps between sends.
- Cross-chip is fine: SX1262 (node) <-> SX1276 (hub) interoperate at the PHY level as long
  as every modulation parameter matches (see below).

## Radio parameters (identical on node and hub)

| Parameter        | Value        |
|------------------|--------------|
| Band             | EU868        |
| Frequency        | 868.1 MHz    |
| Modulation       | LoRa         |
| Spreading factor | SF10         |
| Bandwidth        | 125 kHz      |
| Coding rate      | 4/5          |
| Sync word        | 0x12 (private) |
| Preamble         | 8 symbols    |
| Header           | explicit     |
| CRC              | on           |

SF10 is a conservative default for ~100 m through medium woods to an indoor RX. Once the
hub reports RSSI/SNR + packet-loss in the field, you can drop to SF7-9 to save node battery
(remember to change it on **both** ends).

> **Sync-word gotcha.** SX126x and SX127x encode the sync word differently. RadioLib
> `radio.setSyncWord(0x12)` on the XIAO corresponds to `sync_value: 0x12` on the hub
> (RadioLib maps `0x12 <-> 0x1424` internally). If zero packets are received, this is the
> first thing to check.

## Packet format

Fixed **6-byte payload**, **little-endian**. No application checksum - the LoRa PHY CRC
(enabled above) covers integrity, and the hub ignores CRC-failed frames.

| Offset | Field   | Type  | Encoding                                   |
|--------|---------|-------|--------------------------------------------|
| 0      | node_id | u8    | 1..255, unique per physical node           |
| 1..2   | temp    | int16 | centi-degrees C, signed (value = round(C * 100)) |
| 3..4   | batt_mv | u16   | battery millivolts                         |
| 5      | seq     | u8    | send counter, increments each TX, wraps 255 -> 0 |

Examples (bytes shown in TX order):

- node 1, `21.37 C`, `3.912 V`, seq 42 -> temp=2137 (`0x0859`), mv=3912 (`0x0F48`):
  `01 59 08 48 0F 2A`
- node 2, `-5.20 C`, `4.010 V`, seq 7 -> temp=-520 (`0xFDF8`), mv=4010 (`0x0FAA`):
  `02 F8 FD AA 0F 07`

The hub parses it as:
```cpp
uint8_t  node_id = x[0];
int16_t  temp_ci = (int16_t)(x[1] | (x[2] << 8));   // /100.0 -> deg C
uint16_t batt_mv = (uint16_t)(x[3] | (x[4] << 8));  // /1000.0 -> V
uint8_t  seq     = x[5];
```

## Sender notes (XIAO nRF52840 + Wio-SX1262, RadioLib)

Illustrative - verify signatures against your RadioLib version and the Wio-SX1262 pin map
(NSS / DIO1 / RST / BUSY are defined by Seeed's board files, not assumed here):

```cpp
#include <RadioLib.h>
SX1262 radio = new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY);

void setupRadio() {
  // freq, bw(kHz), sf, cr(=denominator, so 4/5 -> 5), syncWord, powerdBm, preamble
  radio.begin(868.1, 125.0, 10, 5, 0x12, 22, 8);
  radio.setCRC(true);           // enabled by default; keep it on
}

void sendReading(uint8_t nodeId, float tempC, float battV, uint8_t &seq) {
  int16_t  temp_ci = (int16_t) lroundf(tempC * 100.0f);
  uint16_t batt_mv = (uint16_t) lroundf(battV * 1000.0f);
  uint8_t buf[6] = {
    nodeId,
    (uint8_t)(temp_ci & 0xFF), (uint8_t)((temp_ci >> 8) & 0xFF),
    (uint8_t)(batt_mv & 0xFF), (uint8_t)((batt_mv >> 8) & 0xFF),
    seq++,
  };
  radio.transmit(buf, sizeof(buf));
}
```

Implementation checklist:

- **node_id**: hard-code a unique value per node; it is how the hub tells nodes apart.
- **seq**: keep a counter across wake cycles (RTC-retained RAM) so the hub's packet-loss
  stat is meaningful; a reset just looks like one dropped interval, which is fine.
- **Battery**: XIAO nRF52840 reads VBAT via its onboard divider (enable the divider, read
  the ADC, scale). Send true millivolts so `batt_mv / 1000` is a real voltage in HA.
- **Deep sleep**: send, then sleep for your interval. The hub's "Online" sensor tolerates
  long gaps (see below), so pick the interval for battery life, not for the hub.
- **TX power**: SX1262 supports up to +22 dBm; the hub only receives, so its power is N/A.

## What the hub exposes to Home Assistant (per node)

| Entity        | Source                                   |
|---------------|------------------------------------------|
| Temperature   | payload temp / 100                       |
| Battery       | payload batt_mv / 1000 (V)               |
| RSSI          | measured by the hub on each packet (dBm) |
| SNR           | measured by the hub on each packet (dB)  |
| Packet Loss   | EMA (alpha 0.1) of gaps in `seq` (%)     |
| Online        | connectivity; goes offline after 24 h of silence |

- **Packet loss** is derived purely from `seq` gaps, so it needs no knowledge of your send
  interval. A gap of N means N-1 packets were missed. Duplicates (gap 0) and huge gaps
  (> 32, treated as a re-sync after long silence) are ignored so the figure stays smooth.
- **Online** is a 24 h watchdog: every received packet restarts the timer; if nothing
  arrives for 24 h the node is marked offline in HA. Coarse by design.

## Adding another node

All in [lora-remote-temperature.yaml](lora-remote-temperature.yaml), no new files:

1. Add a `node2_id` / `node2_name` substitution pair.
2. Copy the node 1 blocks (5 template sensors, the `binary_sensor`, the two `globals`, the
   `script`) and rename `node_1_*` / `node1_*` -> `node_2_*` / `node2_*`.
3. Add an `else if (nid == 2) { ... }` branch in the `on_packet` lambda mirroring node 1.
4. Flash a XIAO with `node_id = 2`.

## Building / secrets

```bash
esphome compile esphome/remote-temperature/lora-remote-temperature.yaml
```

Needs a `secrets.yaml` **in this directory** (`esphome/remote-temperature/`) with
`wifi_ssid`, `wifi_password`, `api_encryption_key`, `ota_password`. ESPHome resolves
`secrets.yaml` relative to the config file, so a shared one higher up (e.g. `esphome/`)
is not picked up when compiling by path from the repo root.
