#pragma once

#include <Arduino.h>

constexpr int PLAYPAD_MAX_PACKET_SIZE = 0x0020;
constexpr int NUM_PADS = 0x03;

/**
 * @brief A byte enum defining whether or not a packet is a command (aka sent by the game -> pad
 * *or* an "event" which is pad -> game)
 *
 */
enum class PacketType : uint8_t {
    COMMAND = 0x55,
    EVENT = 0x56,
};

/**
 * @brief An enum which describes all of the command types
 *
 */
enum class GatewayCommand : uint8_t {
    // A WAKE packet is used to start the handshaking process with a playpad
    WAKE = 0xB0,
    // A SEED packet is the next step in the handshaking process, and involves setting the PRNG to
    // the correct seed
    SEED = 0xB1,
    // A CHALLENGE packet is the 3rd & final step and involves checking if the PRNG implementation
    // is correct, in addition to the encryption.
    CHALLENGE = 0xB3,

    COL = 0xC0,
    GETCOL = 0xC1,  // TODO: Needs implemented
    FADE = 0xC2,
    FLASH = 0xC3,

    FADRD = 0xC4,
    FADAL = 0xC6,
    FLSAL = 0xC7,
    COLAL = 0xC8,

    TGLST = 0xD0,  // TODO: Needs implemented
    READ = 0xD2,
    WRITE = 0xD3,
    MODEL = 0xD4,

    PWD = 0xE1,     // TODO: Needs implemented
    ACTIVE = 0xE5,  // TODO: Needs implemented

    LEDSEQ = 0xFF,  // TODO: Needs implemented
};

/**
 * @brief A struct for defining R/G/B colors for a specific pad
 *
 */
struct PadColor {
    uint8_t r, g, b;

    /**
     * @brief Returns a pad color based on a 3-byte long buffer
     *
     * @param hex The buffer to read 3 bytes off of and then convert to a color
     * @return PadColor A pad color from the first 3 bytes of the buffer
     */
    static inline PadColor fromBuffer(const uint8_t* hex) { return {hex[0], hex[1], hex[2]}; }

    /**
     * @brief Converts a PadColor to a uint32 like 0xFF00FF
     *
     * @return uint32_t
     */
    inline uint32_t toHex() { return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff); }

    /**
     * @brief Linearly interpolates a color from `a` -> `b` at percentage `t`
     *
     * @param a The starting color
     * @param b The ending color
     * @param t A percentage to interpolate
     * @return PadColor The interpolated color at `t`% between `a` and `b`
     */
    static PadColor lerpColor(const PadColor& a, const PadColor& b, float t) {
        return {
            (uint8_t)(a.r + (int16_t)(b.r - a.r) * t),
            (uint8_t)(a.g + (int16_t)(b.g - a.g) * t),
            (uint8_t)(a.b + (int16_t)(b.b - a.b) * t),
        };
    }

    static constexpr PadColor Black() { return {0x00, 0x00, 0x00}; }
    static constexpr PadColor White() { return {0xFF, 0xFF, 0xFF}; }
    static constexpr PadColor Red() { return {0xFF, 0x00, 0x00}; }
    static constexpr PadColor Green() { return {0x00, 0xFF, 0x00}; }
    static constexpr PadColor Blue() { return {0x00, 0x00, 0xFF}; }

    bool operator==(const PadColor& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const PadColor& o) const { return !(*this == o); }
};

/**
 * @brief An enum detailing all of the locations/values for the specific playpad colors/etc.
 *
 */
enum class PadLocation : uint8_t {
    ALL = 0x00,

    CENTER = 0x01,
    LEFT = 0x02,
    RIGHT = 0x03
};