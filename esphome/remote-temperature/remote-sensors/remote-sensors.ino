// =============================================================================
// LoRa P2P Remote Temperature - Sensor node
// =============================================================================
// Board : Seeed XIAO nRF52840  +  Wio-SX1262 Kit for Meshtastic (SKU 102010710)
// Radio : Semtech SX1262 via RadioLib
// Temp  : DS18B20 on 1-Wire, power-gated so it draws ~0 in sleep
// Role  : Reads temperature + battery, transmits a 6-byte LoRa packet, sleeps.
//         Transmit-only; it never listens. Pairs with the ESPHome hub in
//         ../lora-remote-temperature.yaml.
//
// The radio config and packet layout MUST match the hub byte-for-byte (see
// packet.h and the RADIO section below), or the hub silently drops everything.
//
// Board package: "Seeed nRF52 Boards" (Adafruit nRF52 core). Board = XIAO nRF52840.
// Libraries    : RadioLib, OneWire, DallasTemperature.
// =============================================================================

#include <RadioLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "packet.h"

// --- Identity ---------------------------------------------------------------
static const uint8_t NODE_ID = 1;            // unique per node; matches hub node1_id ("Shed")
static const uint32_t SEND_INTERVAL_MS = 5UL * 60UL * 1000UL;  // 5 min

// --- Pin map (Wio-SX1262 Kit for Meshtastic, non-BTB wiring) -----------------
// Radio pins are made by the stacked kit header; listed here for RadioLib.
static const int PIN_LORA_CS   = D4;
static const int PIN_LORA_DIO1 = D1;
static const int PIN_LORA_RST  = D2;
static const int PIN_LORA_BUSY = D3;
static const int PIN_LORA_RXEN = D5;   // RF-switch RX enable; held low (TX-only node)

// Hand-wired DS18B20 (see README connection diagram).
static const int PIN_DS_DATA = D0;     // 1-Wire data; 4.7k pull-up to PIN_DS_PWR (switched rail)
static const int PIN_DS_PWR  = D6;     // high = sensor powered, low = fully de-energised

// Battery sense is internal to the XIAO (no external wiring).
static const int PIN_VBAT        = 32; // P0.31, VBAT via onboard R17=1M / R18=510k divider
static const int PIN_VBAT_ENABLE = 14; // P0.14, drive LOW to enable the divider read

// --- Radio config: identical to the hub ------------------------------------
// EU868 868.1 MHz, LoRa, SF10, BW 125 kHz, CR 4/5, sync 0x12 (private),
// preamble 8, explicit header, CRC on. 0x12 == RADIOLIB_SX126X_SYNC_WORD_PRIVATE.
static const float   RF_FREQ       = 868.1f;
static const float   RF_BW         = 125.0f;
static const uint8_t RF_SF         = 10;
static const uint8_t RF_CR         = 5;      // coding-rate denominator (4/5)
static const uint8_t RF_SYNCWORD   = 0x12;
static const int8_t  RF_POWER_DBM  = 22;     // SX1262 max
static const uint16_t RF_PREAMBLE  = 8;
// Wio-SX1262 board specifics - both are REQUIRED or the module never transmits:
static const float   RF_TCXO_V     = 1.8f;   // TCXO on DIO3 (not an XTAL)
// (DIO2 gates the TX RF switch -> radio.setDio2AsRfSwitch(true) below)

SX1262 radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

OneWire oneWire(PIN_DS_DATA);
DallasTemperature ds(&oneWire);

static uint8_t g_seq = 0;   // retained across cycles (RAM survives tickless-idle delay())

// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // ADC for battery: 3.0 V reference, 12-bit. Divider enable pin idle-disabled (HIGH).
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, HIGH);

  // DS18B20 power gate: start powered off.
  pinMode(PIN_DS_PWR, OUTPUT);
  digitalWrite(PIN_DS_PWR, LOW);
  pinMode(PIN_DS_DATA, INPUT);

  // RF switch RX side off (TX-only node).
  pinMode(PIN_LORA_RXEN, OUTPUT);
  digitalWrite(PIN_LORA_RXEN, LOW);

  int st = radio.begin(RF_FREQ, RF_BW, RF_SF, RF_CR, RF_SYNCWORD,
                       RF_POWER_DBM, RF_PREAMBLE, RF_TCXO_V, false /* DC-DC */);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.print("radio.begin failed: ");
    Serial.println(st);
    while (true) { delay(1000); }  // wiring/TCXO/pin-order problem - stop here
  }
  radio.setDio2AsRfSwitch(true);   // TX RF switch on DIO2
  radio.setCRC(true);              // LoRa PHY CRC, matches hub
}

// Read the DS18B20 with power gating. Returns false if no valid reading.
bool readTemperatureC(float &out) {
  digitalWrite(PIN_DS_PWR, HIGH);  // power the sensor + its pull-up
  delay(10);                       // let the rail settle
  ds.begin();                      // (re)enumerate after power-up
  ds.requestTemperatures();        // blocks ~750 ms at 12-bit
  float c = ds.getTempCByIndex(0);
  digitalWrite(PIN_DS_PWR, LOW);   // de-energise VDD + pull-up
  pinMode(PIN_DS_DATA, INPUT);     // don't back-feed the powered-down sensor
  if (c == DEVICE_DISCONNECTED_C) return false;
  out = c;
  return true;
}

// Read battery in millivolts. Enable divider before, disable after.
uint16_t readBatteryMv() {
  digitalWrite(PIN_VBAT_ENABLE, LOW);   // enable divider read
  delay(5);
  int raw = analogRead(PIN_VBAT);
  digitalWrite(PIN_VBAT_ENABLE, HIGH);  // disable divider
  // 3.0 V ref / 12-bit -> mV/LSB, then undo the R17/R18 divider (1510k/510k).
  float mv = raw * (3000.0f / 4096.0f) * (1510.0f / 510.0f);
  return (uint16_t) lroundf(mv);
}

void loop() {
  float tempC = 0.0f;
  bool haveTemp = readTemperatureC(tempC);
  uint16_t battMv = readBatteryMv();

  if (haveTemp) {
    int16_t tempCi = (int16_t) lroundf(tempC * 100.0f);
    uint8_t buf[packet::SIZE];
    packet::build(NODE_ID, tempCi, battMv, g_seq, buf);

    int st = radio.transmit(buf, packet::SIZE);   // wakes from sleep, transmits
    if (st == RADIOLIB_ERR_NONE) {
      Serial.print("sent node=");   Serial.print(NODE_ID);
      Serial.print(" temp=");       Serial.print(tempC, 2);
      Serial.print(" batt_mv=");    Serial.print(battMv);
      Serial.print(" seq=");        Serial.println(g_seq);
      g_seq++;                      // advance only on a successful send
    } else {
      Serial.print("transmit failed: ");
      Serial.println(st);
    }
  } else {
    Serial.println("DS18B20 not found - skipping send");
  }

  radio.sleep();                   // radio to low-power (warm sleep) BEFORE the MCU sleeps
  delay(SEND_INTERVAL_MS);         // Adafruit nRF52 core: tickless-idle low-power sleep
}
