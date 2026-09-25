#include "ParticleGraph.h"
#include "ParticleNodes.h"   // pulls in the node registrations (DekiNodeGraph::NodeFactory)

#include <deki/assets/AssetManager.h>
#include <deki/LogSystem.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DekiParticles
{

// Runtime loader for the ParticleGraph effect asset. Mirrors the plain
// data-asset loaders: the editor compiles the ".asset" JSON to a MessagePack
// cache via the generic path, and here we parse that cache with the generic
// DekiNodeGraph::NodeGraphData loader (which creates the node instances via DekiNodeGraph::NodeFactory).
// Same path on desktop and device. A malformed graph loads as nullptr, loudly.

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

    struct _ParticleGraphLoaderReg
    {
        _ParticleGraphLoaderReg()
        {
            // Through the engine's filesystem: the path is a virtual one on a
            // device (F:/assets/..., S:/...), which a std::ifstream cannot open.
            auto pathLoader = [](const char* p) -> void*
            {
                std::vector<uint8_t> buf;
                if (!Deki::AssetManager::ReadWholeFile(p, buf))
                    return nullptr;
                return LoadGraphFromMemory(buf.data(), buf.size());
            };
            auto unloader  = [](void* a) { delete static_cast<ParticleGraph*>(a); };
            auto memLoader = [](const uint8_t* d, size_t n) -> void* { return LoadGraphFromMemory(d, n); };

            Deki::AssetManager::RegisterLoader("ParticleGraph", pathLoader, unloader, memLoader);
        }
    };
    static _ParticleGraphLoaderReg s_particleGraphLoaderReg;
}

}  // namespace DekiParticles
