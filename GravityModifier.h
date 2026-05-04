#pragma once

#include "ParticleModifier.h"

/**
 * @brief Adds a constant acceleration to each particle's velocity. Phase 100.
 *
 * Position integration happens automatically in the emitter after all force
 * modifiers run, so this modifier is optional — emitters without gravity
 * still move particles by their initial velocity.
 */
class DEKI_PARTICLES_API GravityModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(GravityModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4007-9007-000000000015", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(-100.0f, 100.0f)
    DEKI_UNIT(Acceleration)
    float gravityX = 0.0f;          // m/s^2

    DEKI_EXPORT
    DEKI_RANGE(-100.0f, 100.0f)
    DEKI_UNIT(Acceleration)
    float gravityY = -9.8f;         // m/s^2 (world Y+ is up; gravity pulls down)

    int  GetSimulationPhase() const override { return 100; }
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/GravityModifier.gen.h"
