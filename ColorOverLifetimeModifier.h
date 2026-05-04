#pragma once

#include "ParticleModifier.h"
#include "Color.h"

/**
 * @brief Lerps tint color and alpha across a particle's lifetime. Phase 200.
 *
 * Writes into the pool's shared tint columns. The render path multiplies
 * the sprite's pixels by these per-particle tints inside QuadBlit.
 */
class DEKI_PARTICLES_API ColorOverLifetimeModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(ColorOverLifetimeModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-400b-900b-000000000019", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    deki::Color colorAt0 = deki::Color::White;

    DEKI_EXPORT
    deki::Color colorAt1 = deki::Color::Transparent;

    int  GetSimulationPhase() const override { return 200; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/ColorOverLifetimeModifier.gen.h"
