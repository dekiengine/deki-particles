#pragma once

#include "ParticleModifier.h"

/**
 * @brief Sets a particle's initial rotation and rotation speed. Phase 10.
 *
 * Allocates the pool's rotation column on attach. Per-frame integration of
 * rotation by rotationSpeed lives in RotationOverLifetimeModifier — this
 * modifier only sets initial state. Use both together for spinning particles
 * that also taper their spin rate over lifetime, or just this for steady spin.
 */
class DEKI_PARTICLES_API InitialRotationModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(InitialRotationModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4006-9006-000000000014", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(-360.0f, 360.0f)
    float rotationMin = 0.0f;          // degrees

    DEKI_EXPORT
    DEKI_RANGE(-360.0f, 360.0f)
    float rotationMax = 0.0f;

    DEKI_EXPORT
    DEKI_RANGE(-720.0f, 720.0f)
    float spinSpeedMin = 0.0f;         // degrees per second

    DEKI_EXPORT
    DEKI_RANGE(-720.0f, 720.0f)
    float spinSpeedMax = 0.0f;

    int  GetSimulationPhase() const override { return 10; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnEmit(ParticleEmitterComponent& emitter, int i) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/InitialRotationModifier.gen.h"
