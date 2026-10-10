#include "ParticleGraph.h"
#include "ParticleNodes.h"  // pulls in the node registrations (DekiNodeGraph::NodeFactory)

#include <deki/assets/AssetManager.h>
#include <deki/LogSystem.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DekiParticles
{

// Runtime loader for the ParticleGraph effect asset, like the plain data-asset
// loaders: the editor compiles the ".asset" JSON to a MessagePack cache, and
// this parses it with the generic DekiNodeGraph::NodeGraphData loader, which
// creates the node instances through DekiNodeGraph::NodeFactory. The same on
// desktop and device. A malformed graph logs an error and loads as nullptr.

namespace
{
ParticleGraph* LoadGraphFromMemory(const uint8_t* data, size_t size)
{
    DekiNodeGraph::NodeGraphData* graphData = DekiNodeGraph::NodeGraphData::LoadFromMemory(data, size);
    if (!graphData)
    {
        DEKI_LOG_ERROR("ParticleGraph: failed to load particle effect asset");
        return nullptr;
    }

    auto* graph = new ParticleGraph();
    graph->data = graphData;
    return graph;
}

}  // namespace

void RegisterGraphLoader()
{
    // Through the engine's filesystem: the path is a virtual one on a
    // device (F:/assets/..., S:/...), which a std::ifstream cannot open.
    auto pathLoader = [](const char* p) -> void*
    {
        // External, not a std::vector on the internal heap: the file
        // is only held while it is parsed.
        Deki::Buffer<uint8_t> buf;
        if (!Deki::AssetManager::ReadWholeFile(p, buf, Deki::Memory::External))
        {
            return nullptr;
        }
        return LoadGraphFromMemory(buf.Data(), buf.Count());
    };
    auto unloader = [](void* a) { delete static_cast<ParticleGraph*>(a); };
    auto memLoader = [](const uint8_t* d, size_t n) -> void* { return LoadGraphFromMemory(d, n); };

    Deki::AssetManager::RegisterLoader("ParticleGraph", pathLoader, unloader, memLoader);
}

}  // namespace DekiParticles
