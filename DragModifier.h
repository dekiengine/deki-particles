#pragma once

#include "ParticleModifier.h"

/**
 * @brief Linear velocity damping. Phase 110 — runs after gravity so
 *        applied forces aren't immediately undone by drag.
 *
 *        v *= max(0, 1 - drag * dt)
 */
class DEKI_PARTICLES_API DragModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(DragModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4008-9008-000000000016", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    DEKI_RANGE(0.0f, 20.0f)
    float drag = 1.0f;       // 1/sec — at drag=1.0, particles lose ~63% of velocity per second

    int  GetSimulationPhase() const override { return 110; }
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;
};

#include "generated/DragModifier.gen.h"
