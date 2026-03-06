#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include "util/reader.h"

/**
 * @brief A C++ implementation of the [Tiny Encryption Algorithm](https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm).
 *
 */
class TEA
{
public:
    TEA() {}

    /**
     * @brief Set the 16 byte key
     *
     * @param key A key for encryption (16 bytes long)
     */
    void setKey(const uint8_t *key);

    /**
     * @brief Encrypts 8 bytes in-place (optionally, send to an output buffer)
     *
     * @param input 8-byte plaintext buffer
     * @param output 8-byte ciphertext buffer
     * @return true if encryption succeed
     * @return false if key has not been set
     */
    bool encrypt(const uint8_t *input, uint8_t *output);

    /**
     * @brief Decrypts 8 bytes in-place (optionally, send to an output buffer)
     *
     * @param input 8-byte ciphertext buffer
     * @param output 8-byte plaintext buffer
     * @return true if decryption succeed
     * @return false if key has not been set
     */
    bool decrypt(const uint8_t *input, uint8_t *output);

private:
    uint8_t _key[16];
    bool _keySet = false;

    void encipher(uint32_t v[2], const uint32_t k[4], uint32_t out[2]);
    void decipher(uint32_t v[2], const uint32_t k[4], uint32_t out[2]);
};