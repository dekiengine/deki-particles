#pragma once

#include "DekiParticlesAPI.h"
#include <deki/Component.h>  // Deki::HashString, for the registration macro

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace DekiParticles
{

class ParticleEmitterComponent;

/// Runtime behaviour for one particle modifier node type.
///
/// Modifier data lives in the reflected node structs of ParticleNodes.h, which
/// belong to the graph asset and are shared by every emitter using it. So
/// anything a modifier must remember between frames goes in a per-emitter
/// blob: `stateSize` bytes, zeroed when the chain is built and passed to every
/// callback. The same split as the FSM's action library.
///
/// All callbacks are optional. A modifier that only sets up new particles
/// fills in onEmit; one that only moves existing particles fills in
/// onSimulate; one that needs a pool column (rotation, scale, tint) asks for
/// it in onAttach, where the capacity is known.
///
/// isEnabled reads the node's own `enabled` field. It is a function rather
/// than a fixed offset because node structs are plain reflected data with no
/// common base, and a device build has no reflected property table to look the
/// field up in.
struct ParticleModifierOps
{
    size_t stateSize = 0;

    // Once, when the chain is built: set up private state, request pool
    // columns. Called again when the editor preview restarts.
    void (*onAttach)(const void* data, void* state, ParticleEmitterComponent& emitter) = nullptr;

    // A particle was just spawned at index i. Position and velocity are zero;
    // lifetime is valid only if an emission node ahead of this one set it.
    void (*onEmit)(const void* data, void* state, ParticleEmitterComponent& emitter, int i) = nullptr;

    // Once per frame. Iterate the live range [0, pool.AliveCount()).
    // Emission modifiers spawn from here.
    void (*onSimulate)(const void* data, void* state, ParticleEmitterComponent& emitter, float dt) = nullptr;

    // The node's authoring toggle. Null counts as always enabled.
    bool (*isEnabled)(const void* data) = nullptr;
};

/// Maps a typeId (Deki::HashString of the node name) to its runtime ops.
///
/// The data structs register themselves with DekiNodeGraph::NodeFactory
/// through their generated code; this registry holds the behaviour. The
/// emitter refuses, with an error, to run a graph with a "Particles/" node
/// type that has no entry here, rather than skip a modifier without a trace.
class DEKI_PARTICLES_API ParticleModifierRegistry
{
public:
    static ParticleModifierRegistry& Instance();

    void Register(uint32_t typeId, const ParticleModifierOps& ops) { m_Ops[typeId] = ops; }

    const ParticleModifierOps* Find(uint32_t typeId) const
    {
        auto it = m_Ops.find(typeId);
        return it != m_Ops.end() ? &it->second : nullptr;
    }

private:
    ParticleModifierRegistry() = default;
    std::unordered_map<uint32_t, ParticleModifierOps> m_Ops;
};

// Registers runtime ops for a modifier node struct. Place it at file scope in
// a .cpp, next to the callbacks. ClassName must be a DEKI_NODE type; the key
// is the hash of its node name, as the graph loader stores it.
#define REGISTER_PARTICLE_MODIFIER(ClassName, Ops)                                                                     \
    static struct ClassName##_ParticleModifierRegistrar                                                                \
    {                                                                                                                  \
        ClassName##_ParticleModifierRegistrar()                                                                        \
        {                                                                                                              \
            ParticleModifierRegistry::Instance().Register(::Deki::HashString(ClassName::StaticNodeName), Ops);         \
        }                                                                                                              \
    } s_##ClassName##_ParticleModifierRegistrar

}  // namespace DekiParticles
