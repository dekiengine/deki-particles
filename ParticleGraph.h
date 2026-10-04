#pragma once

#include "deki-nodegraph/NodeGraphData.h"

namespace DekiParticles
{

// A particle effect as a loadable asset. The ".asset" (JSON,
// "type":"ParticleGraph") is authored in the editor's Node Graph window and
// compiled to MessagePack by the generic data-asset path. At runtime the
// loader parses it into type-erased node instances (see ParticleNodes.h) and
// the link table. ParticleEmitterComponent walks it once to build its modifier
// chain. Nothing here is editor-only.
//
// One graph drives any number of emitters. The node instances hold the tuning
// values and are shared; each emitter keeps only its own small state blobs.
struct ParticleGraph
{
    // Asset type name for AssetRef<ParticleGraph> and AssetManager lookup.
    // Must match the ".asset" file's "type" field, the runtime loader
    // registration and the editor's node-graph domain registration.
    static constexpr const char* kAssetTypeName = "ParticleGraph";

    DekiNodeGraph::NodeGraphData* data = nullptr;

    ~ParticleGraph() { delete data; }
};

}  // namespace DekiParticles
