#include "playpad/playpad_hid.h"

esp_err_t construct_packet(const uint8_t* data, size_t data_len, uint8_t* packet) {
    if (data_len > PLAYPAD_MAX_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t checksum = 0x00;
    for (int i = 0; i < data_len; i++) {
        checksum += data[i];
        if (checksum >= 256) {
            checksum -= 256;
        }
    }

    memcpy(packet, data, data_len);
    packet[data_len] = checksum;

    size_t total_len = data_len + 1;
    while (total_len < PLAYPAD_MAX_PACKET_SIZE) {
        packet[total_len++] = 0x00;
    }

    return ESP_OK;
}