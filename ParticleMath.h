#pragma once

#include <cstdint>
#include <cmath>

namespace deki_particles {

// xorshift32 PRNG. One state per emitter; cheap and good enough for visuals.
struct Xorshift32
{
    uint32_t state;

    void Seed(uint32_t s) { state = s ? s : 0x9E3779B9u; }

    uint32_t NextU32()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float NextFloat01()
    {
        return (NextU32() >> 8) * (1.0f / 16777216.0f);
    }

    float NextFloatRange(float lo, float hi)
    {
        return lo + (hi - lo) * NextFloat01();
    }
};

// 256-entry sin LUT in Q15 fixed-point (int16). Index is angle * (256/2pi),
// wrapped via & 0xFF.
class TrigLUT
{
public:
    static const int16_t* SinTable()
    {
        static int16_t s_tbl[256] = {0};
        static bool s_init = false;
        if (!s_init) { Build(s_tbl); s_init = true; }
        return s_tbl;
    }

    // Sample sin(radians) as float using the LUT. No interpolation.
    static float Sin(float radians)
    {
        const int16_t* t = SinTable();
        constexpr float k = 256.0f / 6.2831853f;
        int idx = (int)(radians * k);
        return t[idx & 0xFF] * (1.0f / 32767.0f);
    }

    static float Cos(float radians)
    {
        const int16_t* t = SinTable();
        constexpr float k = 256.0f / 6.2831853f;
        int idx = (int)(radians * k) + 64;
        return t[idx & 0xFF] * (1.0f / 32767.0f);
    }

private:
    static void Build(int16_t* t)
    {
        for (int i = 0; i < 256; ++i)
        {
            float a = (i / 256.0f) * 6.2831853f;
            float v = std::sin(a) * 32767.0f;
            if (v >  32767.0f) v =  32767.0f;
            if (v < -32767.0f) v = -32767.0f;
            t[i] = (int16_t)v;
        }
    }
};

inline float DegToRad(float d) { return d * (3.14159265f / 180.0f); }
inline float RadToDeg(float r) { return r * (180.0f / 3.14159265f); }

} // namespace deki_particles
