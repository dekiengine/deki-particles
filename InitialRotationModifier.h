#pragma once

#include "ParticleModifier.h"
#include "DekiMath.h"

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
    DEKI_RANGE(-DekiMath::kTwoPi, DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float rotationMin = 0.0f;          // radians

    DEKI_EXPORT
    DEKI_RANGE(-DekiMath::kTwoPi, DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float rotationMax = 0.0f;

    DEKI_EXPORT
    DEKI_RANGE(-2.0f * DekiMath::kTwoPi, 2.0f * DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float spinSpeedMin = 0.0f;         // radians per second

    DEKI_EXPORT
    DEKI_RANGE(-2.0f * DekiMath::kTwoPi, 2.0f * DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float spinSpeedMax = 0.0f;

    int  GetSimulationPhase() const override { return 10; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnEmit(ParticleEmitterComponent& emitter, int i) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/InitialRotationModifier.gen.h"
