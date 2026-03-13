#pragma once
#include <Arduino.h>
#include "util/reader.h"

/**
 * @brief Raw NFC Tag Buffer, this is an object which contains 45 different 4-byte pages.
 * Each of these tags gets an associated "UID"
 */
class NFCTag {
   public:
    static const uint8_t TAG_SIZE = 180;
    static const uint8_t PAGE_SIZE = 4;
    static const uint8_t PAGES_PER_READ = 4;

    uint8_t _buf[TAG_SIZE];
    uint8_t* data;
    uint8_t dataLen;

    NFCTag() : data(_buf), dataLen(TAG_SIZE) { memset(_buf, 0, TAG_SIZE); }

    NFCTag(const NFCTag& other) : data(_buf), dataLen(other.dataLen) {
        memcpy(_buf, other._buf, TAG_SIZE);
    }

    // Copy assignment operator
    NFCTag& operator=(const NFCTag& other) {
        if (this != &other) {
            dataLen = other.dataLen;
            memcpy(_buf, other._buf, TAG_SIZE);
            data = _buf;  // Always point to OUR buffer, never the source's
        }
        return *this;
    }

    NFCTag(const uint8_t* src, uint8_t len) : data(_buf), dataLen(TAG_SIZE) {
        memset(_buf, 0, TAG_SIZE);
        load(src, len);
    }

    /**
     * @brief Gets the UID as a string, used for saving/JSON serialization
     * Worth noting that this output skips the 3rd BCC/constant byte.
     *
     * @param out
     */
    void getUIDStr(char* out) const {
        for (uint8_t i = 0; i < 3; i++) {
            sprintf(out + (i * 2), "%02X", data[i]);
        }
        for (uint8_t i = 4; i < 8; i++) {
            sprintf(out + ((i - 1) * 2), "%02X", data[i]);
        }
        out[14] = '\0';
    }

    /**
     * @brief Sets the UID based on a uint64_t filled with the respective data
     *
     * @param tag
     */
    void setUID(uint64_t tag) {
        for (uint8_t i = 0; i < 3; i++) {
            _buf[i] = (tag >> (i * 8)) & 0xFF;
        }

        _buf[3] = 0x00;

        for (uint8_t i = 3; i < 7; i++) {
            _buf[i + 1] = (tag >> (i * 8)) & 0xFF;
        }
    }

    /**
     * @brief Returns the UID as an output buffer
     *
     * @param out
     */
    void getUID(uint8_t out[8]) { memcpy(out, this->_buf, 8); }

    /**
     * @brief Sets the UID of the tag to a UID based on a text/hex dump rather than a uint64_t
     *
     * @param str An input string of 7 characters, like "04A3BDFA544280"
     */
    void setUID(const char* str) {
        // Expects exactly 14 hex chars (no separators), e.g. "04A3BDFA544280"
        // Parses them into 7 bytes and maps them into the same positions.
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            return 0;
        };

        uint8_t bytes[7];
        for (uint8_t i = 0; i < 7; i++) {
            bytes[i] = (hexVal(str[i * 2]) << 4) | hexVal(str[i * 2 + 1]);
        }

        memcpy(_buf, bytes, 3);
        _buf[3] = 0x00;
        memcpy(_buf + 4, bytes + 3, 4);
    }

    /**
     * @brief Copies a specific page from the buffer to `out`
     *
     * @param page The specific page index to copy from
     * @param out The output buffer to copy the page from
     * @return true When the copy is successful
     * @return false If the resulting page would overflow the NFC buffer
     */
    bool get(uint8_t page, uint8_t* out) const {
        uint8_t start = page * PAGE_SIZE;
        if (start + PAGE_SIZE > dataLen) {
            return false;
        }
        memcpy(out, data + start, PAGE_SIZE);
        return true;
    }

    /**
     * @brief Sets a specific page to the values from `src`
     *
     * @param page The specific page index to change
     * @param src A buffer of the page values
     * @return true When the page data is updated
     * @return false When writing the page would overflow the NFC buffer
     */
    bool set(uint8_t page, const uint8_t* src) {
        uint8_t start = page * PAGE_SIZE;
        if (start + PAGE_SIZE > dataLen) {
            return false;
        }
        memcpy(data + start, src, PAGE_SIZE);
        return true;
    }

    // ------------------------------------------------------------------
    // load / dump — replaces readFile / writeFile; operate on raw bytes.
    // ------------------------------------------------------------------
    void load(const uint8_t* src, uint8_t len) {
        uint8_t n = (len < TAG_SIZE) ? len : TAG_SIZE;
        memcpy(_buf, src, n);
        dataLen = n;
    }

    uint8_t dump(uint8_t* dest, uint8_t maxLen) const {
        uint8_t n = (dataLen < maxLen) ? dataLen : maxLen;
        memcpy(dest, data, n);
        return n;
    }

    /**
     * @brief Randomly generates a NXP MIFARE UID
     * The first byte is the NXP vendor ID
     * Bytes 1-6 are all randomly generated
     * The 7th byte is a constant (?)
     *
     * @return uint64_t Returns all 7 bytes packed into a uint64_t in little endian order
     */
    static uint64_t randomUID() {
        uint8_t b[8] = {
            0x04,
            static_cast<uint8_t>(random(0, 0xFF)),
            static_cast<uint8_t>(random(0, 0xFF)),
            static_cast<uint8_t>(random(0, 0xFF)),
            static_cast<uint8_t>(random(0, 0xFF)),
            static_cast<uint8_t>(random(0, 0xFF)),
            static_cast<uint8_t>(random(0, 0xFF)),
            0x80,
        };

        uint64_t out = 0;
        for (uint8_t i = 0; i < 8; i++) {
            out |= (uint64_t)b[i] << (i * 8);
        }

        return out;
    }
};