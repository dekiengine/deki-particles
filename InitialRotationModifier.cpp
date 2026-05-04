#include "InitialRotationModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"

void InitialRotationModifier::OnAttachToEmitter(ParticleEmitterComponent& emitter)
{
    emitter.pool.EnsureRotation();
}

void InitialRotationModifier::OnEmit(ParticleEmitterComponent& emitter, int i)
{
    using namespace deki_particles;
    emitter.pool.rotation[i]      = DegToRad(emitter.rng.NextFloatRange(rotationMin, rotationMax));
    emitter.pool.rotationSpeed[i] = DegToRad(emitter.rng.NextFloatRange(spinSpeedMin, spinSpeedMax));
}

void InitialRotationModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    // Integrate spin so per-particle rotationSpeed has effect even when
    // RotationOverLifetimeModifier isn't attached.
    int n = emitter.pool.AliveCount();
    float* rot = emitter.pool.rotation;
    float* spd = emitter.pool.rotationSpeed;
    for (int i = 0; i < n; ++i)
        rot[i] += spd[i] * dt;
}
