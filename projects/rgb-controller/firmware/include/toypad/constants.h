#pragma once

#include <Arduino.h>

constexpr int PLAYPAD_INTERFACE = 0x0000;

constexpr int PLAYPAD_WRITE_EP = 0x0001;
constexpr int PLAYPAD_READ_EP = 0x0081;

constexpr int PLAYPAD_MAX_PACKET = 0x0020;
constexpr int PLAYPAD_DELAY_MS = 100;

enum class PacketType : uint8_t {
    COMMAND = 0x55,
    EVENT = 0x56,
};

/**
 * @brief Commands the host (game) sends to the playpad.
 */
enum class ToypadCommand : uint8_t {
    WAKE = 0xB0,
    SEED = 0xB1,
    CHALLENGE = 0xB3,

    COL = 0xC0,     // Set one pad to a solid color
    GETCOL = 0xC1,  // Get current color of a pad
    FADE = 0xC2,    // Fade one pad to a color
    FLASH = 0xC3,   // Flash one pad
    FADAL = 0xC6,   // Fade all pads (one command)
    FLSAL = 0xC7,   // Flash all pads (one command)
    COLAL = 0xC8,   // Set all pads to their colors (one command)

    TGLST = 0xD0,  // List placed tags
};

/**
 * @brief RGB color for a pad.
 */
struct PadColor {
    uint8_t r, g, b;

    static PadColor fromHex(const char* hex) {
        if (!hex || hex[0] != '#' || strlen(hex) != 7) {
            return Black();
        }
        return fromUint32((uint32_t)strtoul(hex + 1, nullptr, 16));
    }

    static PadColor fromUint32(uint32_t v) {
        return {(uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 8) & 0xFF), (uint8_t)(v & 0xFF)};
    }

    uint32_t toUint32() const { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }

    bool operator==(const PadColor& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const PadColor& o) const { return !(*this == o); }

    static constexpr PadColor Black() { return {0x00, 0x00, 0x00}; }
    static constexpr PadColor White() { return {0xFF, 0xFF, 0xFF}; }
    static constexpr PadColor Red() { return {0xFF, 0x00, 0x00}; }
    static constexpr PadColor Green() { return {0x00, 0xFF, 0x00}; }
    static constexpr PadColor Blue() { return {0x00, 0x00, 0xFF}; }
};

/**
 * @brief Which physical pad section to address.
 */
enum class PadLocation : uint8_t {
    ALL = 0x00,
    CENTER = 0x01,
    LEFT = 0x02,
    RIGHT = 0x03,
};

#define PS3_PLAYPAD_VENDOR_ID 0x0E6F
#define PS3_PLAYPAD_PRODUCT_ID 0x0241

#define X360_PLAYPAD_VENDOR_ID 0x24C6
#define X360_PLAYPAD_PRODUCT_ID 0xFA01

enum class ToypadPlatform : uint8_t { PS3 = 0x00, X360 = 0x01, UNK = 0xFF };

static ToypadPlatform getToypadPlatformFromIDs(uint16_t VID, uint16_t PID) {
    if (VID == X360_PLAYPAD_VENDOR_ID && PID == X360_PLAYPAD_PRODUCT_ID) {
        return ToypadPlatform::X360;
    }
    if (VID == PS3_PLAYPAD_VENDOR_ID && PID == PS3_PLAYPAD_PRODUCT_ID) {
        return ToypadPlatform::PS3;
    }

    return ToypadPlatform::UNK;
}
