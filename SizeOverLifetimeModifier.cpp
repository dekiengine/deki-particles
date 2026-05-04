#include "SizeOverLifetimeModifier.h"
#include "ParticleEmitterComponent.h"

void SizeOverLifetimeModifier::OnAttachToEmitter(ParticleEmitterComponent& emitter)
{
    emitter.pool.EnsureScale();
}

void SizeOverLifetimeModifier::OnSimulate(ParticleEmitterComponent& emitter, float /*dt*/)
{
    int n = emitter.pool.AliveCount();
    float* age = emitter.pool.age;
    float* life = emitter.pool.lifetime;
    float* sc = emitter.pool.scale;
    float s0 = sizeAt0;
    float ds = sizeAt1 - sizeAt0;
    for (int i = 0; i < n; ++i)
    {
        float t = (life[i] > 0.0f) ? (age[i] / life[i]) : 0.0f;
        if (t > 1.0f) t = 1.0f;
        sc[i] = s0 + ds * t;
    }
}
