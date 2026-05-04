#pragma once

#include "ParticleModifier.h"

enum class EmitterShapeKind : uint8_t
{
    Point  = 0,
    Circle = 1,
    Rect   = 2,
};

/**
 * @brief Required modifier on every emitter. Controls when and where particles
 *        spawn, and how long they live. Phase 0.
 *
 * Combines three concerns that always go together:
 *   - Lifetime range  (every spawn needs a lifetime)
 *   - Continuous emission (rate-based, accumulator)
 *   - Burst emission (count + interval, independent of rate)
 *   - Shape (point / circle / rect — sets spawn position)
 *
 * Continuous and burst paths are additive, NOT mutually exclusive — set
 * `emissionRate` for steady output, `burstCount` for puffs, both for
 * "ambient + occasional gust"-style effects.
 */
class DEKI_PARTICLES_API EmissionModifier : public ParticleModifier
{
public:
    DEKI_COMPONENT(EmissionModifier, ParticleModifier, "Particles", "b1e0e1a0-1111-4003-9003-000000000011", "DEKI_FEATURE_PARTICLE_EMITTER")

    // ---- Lifetime ----------------------------------------------------------
    DEKI_GROUP("Lifetime")
    DEKI_EXPORT
    DEKI_RANGE(0.01f, 60.0f)
    float lifetimeMin = 1.0f;

    DEKI_EXPORT
    DEKI_RANGE(0.01f, 60.0f)
    float lifetimeMax = 1.0f;

    // ---- Continuous --------------------------------------------------------
    DEKI_GROUP("Continuous")
    DEKI_EXPORT
    DEKI_RANGE(0, 1000)
    float emissionRate = 20.0f;       // particles/second (0 = disable continuous)

    // ---- Burst -------------------------------------------------------------
    DEKI_GROUP("Burst")
    DEKI_EXPORT
    DEKI_RANGE(0, 1000)
    int32_t burstCount = 0;           // particles per burst (0 = no burst)

    DEKI_EXPORT
    DEKI_RANGE(0, 60)
    float burstInterval = 0.0f;       // seconds between bursts (0 = single burst at start)

    // ---- Shape -------------------------------------------------------------
    // Position is set in emitter-local space and (when worldSpace=true) the
    // emitter's world position is added on spawn.
    DEKI_GROUP("Shape")
    DEKI_EXPORT
    EmitterShapeKind shape = EmitterShapeKind::Point;

    DEKI_EXPORT
    DEKI_VISIBLE_WHEN(shape, Circle)
    DEKI_RANGE(0.0f, 50.0f)
    DEKI_UNIT(Distance)
    float radius = 0.0f;

    DEKI_EXPORT
    DEKI_VISIBLE_WHEN(shape, Rect)
    DEKI_RANGE(0.0f, 50.0f)
    DEKI_UNIT(Distance)
    float width  = 0.0f;

    DEKI_EXPORT
    DEKI_VISIBLE_WHEN(shape, Rect)
    DEKI_RANGE(0.0f, 50.0f)
    DEKI_UNIT(Distance)
    float height = 0.0f;

    int  GetSimulationPhase() const override { return 0; }
    void OnAttachToEmitter(ParticleEmitterComponent& emitter) override;
    void OnEmit(ParticleEmitterComponent& emitter, int i) override;
    void OnSimulate(ParticleEmitterComponent& emitter, float dt) override;

private:
    float m_RateAccumulator = 0.0f;
    float m_BurstTimer = 0.0f;
    bool  m_FiredFirstBurst = false;
};

#include "generated/EmissionModifier.gen.h"
