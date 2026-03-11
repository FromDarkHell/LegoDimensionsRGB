#pragma once
#include <Arduino.h>
#include "util/reader.h"

/**
 * @brief Raw NFC tag buffer — handles low-level page read/write and UID parsing.
 *        Renamed from Tag. All other tag types inherit from this.
 */
class NFCTag
{
public:
    static const uint8_t TAG_SIZE = 180;
    static const uint8_t PAGE_SIZE = 4;
    static const uint8_t PAGES_PER_READ = 4;

    uint8_t _buf[TAG_SIZE];
    uint8_t *data;
    uint8_t dataLen;

    NFCTag() : data(_buf), dataLen(TAG_SIZE)
    {
        memset(_buf, 0, TAG_SIZE);
    }

    NFCTag(const NFCTag &other) : data(_buf), dataLen(other.dataLen)
    {
        memcpy(_buf, other._buf, TAG_SIZE);
    }

    // Copy assignment operator
    NFCTag &operator=(const NFCTag &other)
    {
        if (this != &other)
        {
            dataLen = other.dataLen;
            memcpy(_buf, other._buf, TAG_SIZE);
            data = _buf; // Always point to OUR buffer, never the source's
        }
        return *this;
    }

    NFCTag(const uint8_t *src, uint8_t len) : data(_buf), dataLen(TAG_SIZE)
    {
        memset(_buf, 0, TAG_SIZE);
        load(src, len);
    }

    // ------------------------------------------------------------------
    // uid() — mirrors JS: data[0..2] + data[4..7] (skips BCC at [3])
    // `out` must be at least 15 bytes (14 hex chars + null terminator).
    // ------------------------------------------------------------------
    void getUIDStr(char *out) const
    {
        for (uint8_t i = 0; i < 3; i++)
            sprintf(out + (i * 2), "%02X", data[i]);
        for (uint8_t i = 4; i < 8; i++)
            sprintf(out + ((i - 1) * 2), "%02X", data[i]);
        out[14] = '\0';
    }

    void setUID(uint64_t tag)
    {
        // Unpack the 8 little-endian bytes and write them into the buffer,
        // preserving the same layout as uid() reads:
        //   positions 0-2  → bits  0..23
        //   position  3    → BCC (leave as 0x00, written by the reader)
        //   positions 4-7  → bits 24..55  (bytes 3-6 of the uint64)
        for (uint8_t i = 0; i < 3; i++)
            _buf[i] = (tag >> (i * 8)) & 0xFF;

        _buf[3] = 0x00; // BCC placeholder — not encoded in the UID value

        for (uint8_t i = 3; i < 7; i++)
            _buf[i + 1] = (tag >> (i * 8)) & 0xFF;
    }

    void getUID(uint8_t out[8])
    {
        memcpy(out, this->_buf, 8);
    }

    void setUID(const char *str)
    {
        // Expects exactly 14 hex chars (no separators), e.g. "04A3BDFA544280"
        // Parses them into 7 bytes and maps them into the same positions.
        auto hexVal = [](char c) -> uint8_t
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return 0;
        };

        uint8_t bytes[7];
        for (uint8_t i = 0; i < 7; i++)
            bytes[i] = (hexVal(str[i * 2]) << 4) | hexVal(str[i * 2 + 1]);

        // bytes[0..2]  → _buf[0..2]
        // BCC gap      → _buf[3] = 0x00
        // bytes[3..6]  → _buf[4..7]
        memcpy(_buf, bytes, 3);
        _buf[3] = 0x00;
        memcpy(_buf + 4, bytes + 3, 4);
    }

    // ------------------------------------------------------------------
    // get(page, out) — copies PAGE_SIZE bytes for `page` into `out`.
    // Returns false if out of range.
    // ------------------------------------------------------------------
    bool get(uint8_t page, uint8_t *out) const
    {
        uint8_t start = page * PAGE_SIZE;
        if (start + PAGE_SIZE > dataLen)
            return false;
        memcpy(out, data + start, PAGE_SIZE);
        return true;
    }

    // ------------------------------------------------------------------
    // set(page, src) — writes PAGE_SIZE bytes from `src` into `page`.
    // Returns false if out of range.
    // ------------------------------------------------------------------
    bool set(uint8_t page, const uint8_t *src)
    {
        uint8_t start = page * PAGE_SIZE;
        if (start + PAGE_SIZE > dataLen)
            return false;
        memcpy(data + start, src, PAGE_SIZE);
        return true;
    }

    // ------------------------------------------------------------------
    // load / dump — replaces readFile / writeFile; operate on raw bytes.
    // ------------------------------------------------------------------
    void load(const uint8_t *src, uint8_t len)
    {
        uint8_t n = (len < TAG_SIZE) ? len : TAG_SIZE;
        memcpy(_buf, src, n);
        dataLen = n;
    }

    uint8_t dump(uint8_t *dest, uint8_t maxLen) const
    {
        uint8_t n = (dataLen < maxLen) ? dataLen : maxLen;
        memcpy(dest, data, n);
        return n;
    }

    // ------------------------------------------------------------------
    // randomUID() — generates a plausible NXP MIFARE Ultralight UID.
    //   Byte 0 : 0x04 (NXP vendor ID)
    //   Bytes 1-6 : random
    //   Byte 7 : 0x80 (observed constant in real tags)
    // Returns the 7 bytes packed into a uint64_t (little-endian byte order).
    // ------------------------------------------------------------------
    static uint64_t randomUID()
    {
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
        for (uint8_t i = 0; i < 8; i++)
            out |= (uint64_t)b[i] << (i * 8);

        return out;
    }
};