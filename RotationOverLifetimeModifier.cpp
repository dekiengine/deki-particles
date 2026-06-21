#include "RotationOverLifetimeModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"

void RotationOverLifetimeModifier::OnAttachToEmitter(ParticleEmitterComponent& emitter)
{
    emitter.pool.EnsureRotation();
}

void RotationOverLifetimeModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    int n = emitter.pool.AliveCount();
    // rotation column is float radians (engine convention); speeds are
    // radians/sec, so integration is a simple unit-agnostic accumulate.
    float a0 = spinSpeedAt0;
    float a1 = spinSpeedAt1;
    float da = ((a1) - (a0));
    float dtN = static_cast<float>(dt);
    float* age  = emitter.pool.age;
    float* life = emitter.pool.lifetime;
    float* rot  = emitter.pool.rotation;
    for (int i = 0; i < n; ++i)
    {
        float t = (life[i] > 0.0f)
            ? ((age[i]) / (life[i]))
            : 0.0f;
        if (t > 1.0f) t = 1.0f;
        float spd = ((a0) + (((da) * (t))));
        rot[i] = ((rot[i]) + (((spd) * (dtN))));
    }
}
