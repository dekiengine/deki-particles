#pragma once

#include "ParticleModifier.h"
#include "DekiMath.h"

/**
 * @brief Sets a particle's initial velocity from a polar speed/angle range. Phase 10.
 */
class DEKI_PARTICLES_API InitialVelocityModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(InitialVelocityModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4005-9005-000000000013", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(-50.0f, 50.0f)
    DEKI_UNIT(Velocity)
    float speedMin = 2.0f;            // m/s

    DEKI_EXPORT
    DEKI_RANGE(-50.0f, 50.0f)
    DEKI_UNIT(Velocity)
    float speedMax = 2.0f;            // m/s

    DEKI_EXPORT
    DEKI_RANGE(-DekiMath::kTwoPi, 2.0f * DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float angleMin = 0.0f;            // radians, 0 = +X right, π/2 = +Y up

    DEKI_EXPORT
    DEKI_RANGE(-DekiMath::kTwoPi, 2.0f * DekiMath::kTwoPi)
    DEKI_UNIT(Angle)
    float angleMax = DekiMath::kTwoPi;

    int  GetSimulationPhase() const override { return 10; }
    void OnEmit(ParticleEmitterComponent& emitter, int i) override;
};

#include "generated/InitialVelocityModifier.gen.h"
