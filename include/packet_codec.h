#ifndef PACKET_CODEC_H
#define PACKET_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculates CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final XOR).
 * Bit-for-bit identical to Dart PacketCodec.crc16() in cnams_app.
 */
uint16_t packet_crc16(const uint8_t *data, size_t length);

/**
 * Encodes a measurement reading into an 11-byte big-endian BLE frame.
 * 
 * Frame Format:
 *   [0]      Start Marker 0xA5
 *   [1]      Channel (0x00 = Weight in grams, 0x01 = Length in mm)
 *   [2]      Flags (bit 0 = Stable: 0x01, Unstable: 0x00)
 *   [3..6]   Value (int32 big-endian)
 *   [7..8]   Sequence Counter (uint16 big-endian)
 *   [9..10]  CRC-16/CCITT-FALSE over bytes [0..8] (uint16 big-endian)
 */
void packet_encode(uint8_t channel, bool stable, int32_t value_raw, uint16_t sequence, uint8_t *out_frame);

/**
 * Validates whether an 11-byte frame has a valid start marker and matching CRC.
 */
bool packet_validate(const uint8_t *frame, size_t length);

#ifdef __cplusplus
}
#endif

#endif // PACKET_CODEC_H
