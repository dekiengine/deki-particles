#pragma once

#include "DekiBehaviour.h"
#include "DekiParticlesAPI.h"
#include "reflection/DekiProperty.h"

class ParticleEmitterComponent;

/**
 * @brief Abstract base for any component that modifies a particle emitter.
 *
 * A ParticleModifier is a normal DekiBehaviour attached to the same
 * DekiObject as a ParticleEmitterComponent. The emitter discovers its
 * sibling modifiers in Start() and drives their hooks each frame.
 *
 * Modifiers do NOT call SetNeedsUpdate(true). The engine must not call
 * Update() on them — the emitter is the sole driver.
 *
 * Phase ordering (lower runs first):
 *    0   Emission (spawns particles into [oldCount, aliveCount))
 *   10   Spawn shape, initial velocity, initial rotation, initial color/scale
 *  100   Forces (gravity, drag)
 *  200   Lifetime curves (color/size/rotation over lifetime)
 */
class DEKI_PARTICLES_API ParticleModifier : public DekiBehaviour
{
public:
    DEKI_COMPONENT(ParticleModifier, DekiBehaviour, "Particles", "b1e0e1a0-1111-4001-9001-000000000001", "DEKI_FEATURE_PARTICLE_EMITTER")

    ParticleModifier() = default;
    virtual ~ParticleModifier() = default;

    // Sort key WITHIN a phase. Lower runs first. Driven by the inspector's
    // reorder arrows. Phase remains the primary ordering authority — `order`
    // only resolves ties.
    DEKI_EXPORT
    int32_t order = 0;

    // When false, the emitter skips this modifier's hooks. Lets users
    // toggle behaviors on/off without losing their tuned values.
    DEKI_EXPORT
    bool enabled = true;

    // Called once when the emitter discovers this modifier. Use to allocate
    // private per-particle state (e.g. std::vector sized to capacity).
    // Pool extension columns (rotation/scale/tint) should be Ensure*-d here.
    virtual void OnAttachToEmitter(ParticleEmitterComponent& /*emitter*/) {}

    // Initialize a freshly spawned particle at index `i`. Position/velocity
    // are zero; lifetime/age are valid only if EmissionModifier set them
    // (it runs at phase 0, before any other OnEmit at phase >= 10).
    virtual void OnEmit(ParticleEmitterComponent& /*emitter*/, int /*i*/) {}

    // Per-frame simulation step. Iterate alive range [0, pool.AliveCount()).
    // Emission modifiers may also spawn particles here.
    virtual void OnSimulate(ParticleEmitterComponent& /*emitter*/, float /*dt*/) {}

    // Phase ordering — lower runs first. Override per modifier.
    virtual int  GetSimulationPhase() const = 0;

protected:
    // Built-in modifiers should never request per-frame Update — they're
    // driven by the emitter. Lock SetNeedsUpdate down for safety.
    using DekiBehaviour::SetNeedsUpdate;
};

// Generated property metadata
#include "generated/ParticleModifier.gen.h"
