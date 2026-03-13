#pragma once

#include <esp_err.h>
#include <cstring>

#define PLAYPAD_VENDOR_ID 0x0E6F
#define PLAYPAD_PRODUCT_ID 0x0241

#define PLAYPAD_INTERFACE 0x0000

#define PLAYPAD_WRITE_ENDPOINT 0x0001
#define PLAYPAD_READ_ENDPOINT 0x0081

#define PLAYPAD_MAX_PACKET_SIZE 0x0020

#define PLAYPAD_DELAY 100

/// @brief A series of bytes which need to be sent in order for the playpad to start proper
/// communication with any USB Host Note: I personally have no clue what any of these do/mean,
/// otherwise, I'd just implement it like that lol
const uint8_t STARTUP_PACKET[] = {
    0x55, 0x0F, 0xB0, 0x01, 0x28, 0x63, 0x29, 0x20, 0x4C, 0x45, 0x47, 0x4F, 0x20, 0x32, 0x30, 0x31,
    0x34, 0xF7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define STARTUP_PACKET_SIZE sizeof(STARTUP_PACKET)

/// @brief A simple function which takes a command packet and converts it to a legal USB playpad
/// packet by adding the checksum and padding if necessary.
/// @param data An array of bytes of the command packet to send to the playpad
/// @param data_len Size of `data`
/// @param packet An array of bytes to put the output packet into
/// @return An error code representing whether or not this packet could be legally constructed
esp_err_t construct_packet(const uint8_t*, size_t, uint8_t*);

typedef unsigned char playpad_pad;

struct playpad_color {
    unsigned char red;
    unsigned char green;
    unsigned char blue;

    bool operator==(const playpad_color& other) const {
        return red == other.red && green == other.green && blue == other.blue;
    }

    bool operator!=(const playpad_color& other) const { return !(*this == other); }

    static playpad_color from_hex(const char* hex) {
        if (!hex || hex[0] != '#' || strlen(hex) != 7) {
            playpad_color color{0, 0, 0};

            return color;
        }

        unsigned int value = strtoul(hex + 1, nullptr, 16);

        return from_uint(value);
    }

    static playpad_color from_uint(const unsigned int value) {
        playpad_color color{0, 0, 0};

        color.red = (value >> 16) & 0xFF;
        color.green = (value >> 8) & 0xFF;
        color.blue = value & 0xFF;

        return color;
    }

    const unsigned int to_uint() {
        return ((static_cast<uint32_t>(this->red) << 16) | (static_cast<uint32_t>(this->green) << 8)
                | (static_cast<uint32_t>(this->blue)));
    }
};

#define PLAYPAD_PAD_ALL 0x00
#define PLAYPAD_PAD_MIDDLE 0x01
#define PLAYPAD_PAD_LEFT 0x02
#define PLAYPAD_PAD_RIGHT 0x03
