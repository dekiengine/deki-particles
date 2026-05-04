#pragma once

#include "ParticleModifier.h"

/**
 * @brief Drives a particle's scale as a function of its normalized age.
 *        Phase 200.
 *
 * Lerps from sizeAt0 to sizeAt1 by tNorm. v1 uses linear interpolation
 * directly — switching to a 32-entry baked LUT becomes worthwhile only if
 * a non-linear curve is added later, since linear lerp is one mul-add.
 */
class DEKI_PARTICLES_API SizeOverLifetimeModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(SizeOverLifetimeModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-400a-900a-000000000018", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(0.0f, 10.0f)
    float sizeAt0 = 1.0f;

    DEKI_EXPORT
    DEKI_RANGE(0.0f, 10.0f)
    float sizeAt1 = 1.0f;

    int  GetSimulationPhase() const override { return 200; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/SizeOverLifetimeModifier.gen.h"
