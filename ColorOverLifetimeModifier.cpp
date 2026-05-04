#include "ColorOverLifetimeModifier.h"
#include "ParticleEmitterComponent.h"

void ColorOverLifetimeModifier::OnAttachToEmitter(ParticleEmitterComponent& emitter)
{
    emitter.pool.EnsureTint();
}

void ColorOverLifetimeModifier::OnSimulate(ParticleEmitterComponent& emitter, float /*dt*/)
{
    int n = emitter.pool.AliveCount();
    float* age = emitter.pool.age;
    float* life = emitter.pool.lifetime;
    uint8_t* tR = emitter.pool.tintR;
    uint8_t* tG = emitter.pool.tintG;
    uint8_t* tB = emitter.pool.tintB;
    uint8_t* tA = emitter.pool.tintA;

    int r0 = colorAt0.r, g0 = colorAt0.g, b0 = colorAt0.b, a0 = colorAt0.a;
    int dr = (int)colorAt1.r - r0;
    int dg = (int)colorAt1.g - g0;
    int db = (int)colorAt1.b - b0;
    int da = (int)colorAt1.a - a0;

    for (int i = 0; i < n; ++i)
    {
        float t = (life[i] > 0.0f) ? (age[i] / life[i]) : 0.0f;
        if (t > 1.0f) t = 1.0f;
        // 8.8 fixed-point lerp avoids float-to-int truncation pattern in the
        // hot loop on MCUs without fast int-from-float.
        int ti = (int)(t * 256.0f);
        tR[i] = (uint8_t)(r0 + ((dr * ti) >> 8));
        tG[i] = (uint8_t)(g0 + ((dg * ti) >> 8));
        tB[i] = (uint8_t)(b0 + ((db * ti) >> 8));
        tA[i] = (uint8_t)(a0 + ((da * ti) >> 8));
    }
}
