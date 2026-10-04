#pragma once

#include "ParticleModifierRegistry.h"

#include <cstdint>
#include <deki/providers/Memory.h>
#include <vector>

namespace DekiParticles
{

// Walks a particle graph into a flat list of modifier callbacks.
//
// Two callers: the runtime emitter walking a loaded ParticleGraph asset, and
// the editor preview walking the live document being edited. Both graph types
// answer FindFirstOfType(typeId) and Next(nodeId, pin) and expose nodes with
// {id, instance}, so the walk is one template and the two cannot drift apart.

// One modifier in a built chain: the node's shared authoring data, the owner's
// private state blob, and the behaviour to run.
struct ParticleChainEntry
{
    const void* data = nullptr;  // node instance (owned by the graph)
    void* state = nullptr;       // stateSize bytes, owned by the chain
    const ParticleModifierOps* ops = nullptr;
};

inline void FreeParticleChain(std::vector<ParticleChainEntry>& chain)
{
    for (ParticleChainEntry& e : chain)
    {
        Deki::Memory::Free(e.state);
    }
    chain.clear();
}

// Loop guard for a hand-edited file that wires a cycle. A real effect is a
// handful of modifiers; nothing legitimate comes close.
constexpr int kMaxParticleChainLength = 256;

/// Walks from the Emitter node forward, one output pin per hop; wire order is
/// execution order. Returns false with *outError set, and nothing built, when
/// there is no entry node or a node names a modifier type with no registered
/// behaviour. Skipping such a modifier would hide the mistake.
template <typename GraphT>
bool BuildParticleChain(const GraphT& graph, uint32_t entryTypeId, std::vector<ParticleChainEntry>& out,
                        const char** outError)
{
    FreeParticleChain(out);

    const auto* node = graph.FindFirstOfType(entryTypeId);
    if (!node)
    {
        *outError = "graph has no Emitter node - nothing to run";
        return false;
    }

    for (int step = 0; step < kMaxParticleChainLength; ++step)
    {
        const auto* next = graph.Next(node->id, 0);
        if (!next)
        {
            return !out.empty();  // end of the chain
        }

        const ParticleModifierOps* ops = ParticleModifierRegistry::Instance().Find(next->typeId);
        if (!ops)
        {
            FreeParticleChain(out);
            *outError = "graph uses a modifier type with no runtime behavior registered";
            return false;
        }

        ParticleChainEntry e;
        e.data = next->instance;
        e.ops = ops;
        // Zeroed: every modifier's state starts at "nothing has happened
        // yet", which is what a fresh accumulator or latch means. Deki::Memory
        // zeroes what it returns. A modifier whose state does not fit is
        // refused rather than run uninitialised.
        e.state =
            ops->stateSize ? Deki::Memory::AllocateArray<uint8_t>(ops->stateSize, Deki::Memory::Internal) : nullptr;
        if (ops->stateSize && !e.state)
        {
            FreeParticleChain(out);
            *outError = "no room for a modifier's state";
            return false;
        }
        out.push_back(e);

        node = next;
    }

    FreeParticleChain(out);
    *outError = "graph chain is longer than the guard allows (wired in a cycle?)";
    return false;
}

}  // namespace DekiParticles
