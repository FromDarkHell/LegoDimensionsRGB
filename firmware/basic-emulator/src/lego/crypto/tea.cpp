#include "lego/crypto/tea.h"
#include "util/reader.h"

#include <string.h>

void TEA::encipher(uint32_t v[2], const uint32_t k[4], uint32_t out[2])
{
    uint32_t v0 = v[0];
    uint32_t v1 = v[1];
    uint32_t sum = 0;
    const uint32_t delta = 0x9E3779B9;
    uint32_t k0 = k[0];
    uint32_t k1 = k[1];
    uint32_t k2 = k[2];
    uint32_t k3 = k[3];

    for (int i = 0; i < 32; i++)
    {
        sum += delta;
        v0 += ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        v1 += ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
    }

    out[0] = v0;
    out[1] = v1;
}

void TEA::decipher(uint32_t v[2], const uint32_t k[4], uint32_t out[2])
{
    uint32_t v0 = v[0];
    uint32_t v1 = v[1];
    uint32_t sum = 0xC6EF3720;
    const uint32_t delta = 0x9E3779B9;
    uint32_t k0 = k[0];
    uint32_t k1 = k[1];
    uint32_t k2 = k[2];
    uint32_t k3 = k[3];

    for (int i = 0; i < 32; i++)
    {
        v1 -= ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
        v0 -= ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        sum -= delta;
    }

    out[0] = v0;
    out[1] = v1;
}

void TEA::setKey(const uint8_t *key)
{
    memcpy(_key, key, 16);
    _keySet = true;
}

bool TEA::encrypt(const uint8_t *input, uint8_t *output)
{
    if (!_keySet)
        return false;

    uint32_t v[2];
    v[0] = Reader::readUInt32LE(input, 0);
    v[1] = Reader::readUInt32LE(input, 4);

    uint32_t k[4];
    k[0] = Reader::readUInt32LE(_key, 0);
    k[1] = Reader::readUInt32LE(_key, 4);
    k[2] = Reader::readUInt32LE(_key, 8);
    k[3] = Reader::readUInt32LE(_key, 12);

    uint32_t out[2];
    encipher(v, k, out);

    Reader::writeUInt32LE(output, 0, out[0]);
    Reader::writeUInt32LE(output, 4, out[1]);

    return true;
}

bool TEA::decrypt(const uint8_t *input, uint8_t *output)
{
    if (!_keySet)
        return false;

    uint32_t v[2];
    v[0] = Reader::readUInt32LE(input, 0);
    v[1] = Reader::readUInt32LE(input, 4);

    uint32_t k[4];
    k[0] = Reader::readUInt32LE(_key, 0);
    k[1] = Reader::readUInt32LE(_key, 4);
    k[2] = Reader::readUInt32LE(_key, 8);
    k[3] = Reader::readUInt32LE(_key, 12);

    uint32_t out[2];
    decipher(v, k, out);

    Reader::writeUInt32LE(output, 0, out[0]);
    Reader::writeUInt32LE(output, 4, out[1]);

    return true;
}