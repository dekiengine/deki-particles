#pragma once

#include "ParticleModifier.h"

/**
 * @brief Drives a particle's spin rate as a function of its normalized age.
 *        Phase 200.
 *
 * Lerps each particle's rotationSpeed between speedAt0 and speedAt1 by
 * tNorm = age/lifetime, then integrates rotation. Use together with
 * InitialRotationModifier for a starting offset, or alone for spin that
 * always starts at zero and ramps up/down.
 */
class DEKI_PARTICLES_API RotationOverLifetimeModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(RotationOverLifetimeModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4009-9009-000000000017", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(-720.0f, 720.0f)
    float spinSpeedAt0 = 0.0f;          // degrees/sec at birth

    DEKI_EXPORT
    DEKI_RANGE(-720.0f, 720.0f)
    float spinSpeedAt1 = 0.0f;          // degrees/sec at death

    int  GetSimulationPhase() const override { return 200; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/RotationOverLifetimeModifier.gen.h"
