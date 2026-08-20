#include "packet_codec.h"
#include "config.h"

uint16_t packet_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] & 0xFF) << 8;
        for (int b = 0; b < 8; b++) {
            if ((crc & 0x8000) != 0) {
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
            } else {
                crc = (crc << 1) & 0xFFFF;
            }
        }
    }
    return crc;
}

void packet_encode(uint8_t channel, bool stable, int32_t value_raw, uint16_t sequence, uint8_t *out_frame) {
    if (!out_frame) return;

    // [0] Start marker
    out_frame[0] = FRAME_START_MARKER;

    // [1] Channel
    out_frame[1] = channel;

    // [2] Flags (bit 0 = stable)
    out_frame[2] = stable ? FLAG_STABLE : FLAG_UNSTABLE;

    // [3..6] Value (int32, Big-Endian)
    out_frame[3] = (uint8_t)((value_raw >> 24) & 0xFF);
    out_frame[4] = (uint8_t)((value_raw >> 16) & 0xFF);
    out_frame[5] = (uint8_t)((value_raw >> 8) & 0xFF);
    out_frame[6] = (uint8_t)(value_raw & 0xFF);

    // [7..8] Sequence (uint16, Big-Endian)
    out_frame[7] = (uint8_t)((sequence >> 8) & 0xFF);
    out_frame[8] = (uint8_t)(sequence & 0xFF);

    // [9..10] CRC-16 over bytes 0..8 (uint16, Big-Endian)
    uint16_t crc = packet_crc16(out_frame, 9);
    out_frame[9]  = (uint8_t)((crc >> 8) & 0xFF);
    out_frame[10] = (uint8_t)(crc & 0xFF);
}

bool packet_validate(const uint8_t *frame, size_t length) {
    if (!frame || length != FRAME_LENGTH) return false;
    if (frame[0] != FRAME_START_MARKER) return false;

    uint16_t expected_crc = packet_crc16(frame, 9);
    uint16_t actual_crc = (((uint16_t)frame[9]) << 8) | (uint16_t)frame[10];

    return (expected_crc == actual_crc);
}
