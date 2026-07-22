// Packet encoder for the LoRa remote-temperature contract.
//
// Byte-for-byte mirror of the hub decoder in ../lora-remote-temperature.yaml:
//     uint8_t  node_id = x[0];
//     int16_t  temp_ci = (int16_t)(x[1] | (x[2] << 8));   // /100.0 -> deg C
//     uint16_t batt_mv = (uint16_t)(x[3] | (x[4] << 8));  // /1000.0 -> V
//     uint8_t  seq     = x[5];
//
// Fixed 6-byte, little-endian payload. No app checksum - the LoRa PHY CRC covers integrity.
// Pure C++ (no Arduino deps) so it can be unit-tested on the host.

#ifndef REMOTE_SENSORS_PACKET_H
#define REMOTE_SENSORS_PACKET_H

#include <stdint.h>
#include <stddef.h>

namespace packet {

// Wire layout.
static const size_t SIZE = 6;
enum Offset {
  OFF_NODE_ID = 0,
  OFF_TEMP_LO = 1,  // int16 LE, centi-degrees C
  OFF_TEMP_HI = 2,
  OFF_BATT_LO = 3,  // u16 LE, millivolts
  OFF_BATT_HI = 4,
  OFF_SEQ = 5,
};

// Encode one reading into out[6]. temp_ci is centi-degrees C (round(C * 100)), signed.
inline void build(uint8_t node_id, int16_t temp_ci, uint16_t batt_mv, uint8_t seq,
                  uint8_t out[SIZE]) {
  uint16_t t = (uint16_t) temp_ci;  // two's-complement bit pattern, LE
  out[OFF_NODE_ID] = node_id;
  out[OFF_TEMP_LO] = (uint8_t)(t & 0xFF);
  out[OFF_TEMP_HI] = (uint8_t)((t >> 8) & 0xFF);
  out[OFF_BATT_LO] = (uint8_t)(batt_mv & 0xFF);
  out[OFF_BATT_HI] = (uint8_t)((batt_mv >> 8) & 0xFF);
  out[OFF_SEQ] = seq;
}

}  // namespace packet

#endif  // REMOTE_SENSORS_PACKET_H
