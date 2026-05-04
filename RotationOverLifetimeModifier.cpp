#include "RotationOverLifetimeModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"

void RotationOverLifetimeModifier::OnAttachToEmitter(ParticleEmitterComponent& emitter)
{
    emitter.pool.EnsureRotation();
}

void RotationOverLifetimeModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    using namespace deki_particles;
    int n = emitter.pool.AliveCount();
    float a0 = DegToRad(spinSpeedAt0);
    float a1 = DegToRad(spinSpeedAt1);
    float* age = emitter.pool.age;
    float* life = emitter.pool.lifetime;
    float* rot = emitter.pool.rotation;
    for (int i = 0; i < n; ++i)
    {
        float t = (life[i] > 0.0f) ? (age[i] / life[i]) : 0.0f;
        if (t > 1.0f) t = 1.0f;
        float spd = a0 + (a1 - a0) * t;
        rot[i] += spd * dt;
    }
}
