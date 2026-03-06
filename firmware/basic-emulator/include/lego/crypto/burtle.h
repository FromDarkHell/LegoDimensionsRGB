#pragma once

#include <Arduino.h>

/**
 * @brief An implementation of Bob Jenkin's small PRNG available here: https://burtleburtle.net/bob/rand/smallprng.html
 *
 */
class Burtle
{
public:
    struct State
    {
        uint32_t a, b, c, d;
    };

    Burtle() : x{0, 0, 0, 0} {}

    void init(uint32_t seed)
    {
        x.a = 0xf1ea5eed;
        x.b = x.c = x.d = seed;
        for (int i = 0; i < 42; ++i)
            rand();
    }

    uint32_t rand()
    {
        auto rot = [](uint32_t a, uint32_t b) -> uint32_t
        {
            return (a << b) | (a >> (32 - b));
        };
        uint32_t e = x.a - rot(x.b, 21);
        x.a = x.b ^ rot(x.c, 19);
        x.b = x.c + rot(x.d, 6);
        x.c = x.d + e;
        x.d = e + x.a;
        return x.d;
    }

private:
    State x;
};