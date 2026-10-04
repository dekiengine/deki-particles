// Editor registration for the ParticleGraph effect asset:
// (a) the asset type, so the Asset Browser's Create... menu offers "Particle
//     Effect" with a valid starting graph;
// (b) the node-graph domain, so the generic Node Graph window opens this
//     asset type and limits its add-node menu to the "Particles" categories
//     (see ParticleNodes.h).
// Compiling needs no code here: the type has a runtime loader, so the generic
// data-asset path converts the JSON to a MessagePack cache.

#ifdef DEKI_EDITOR

#include <deki-editor/EditorExtension.h>
#include <deki-editor/EditorRegistry.h>

#include "deki-nodegraph/DekiNode.h"

// Editor extensions live in DekiEditor; the package's own types are in DekiParticles.
using namespace DekiParticles;

namespace DekiEditor
{

class ParticleGraphAssetEditor : public AssetTypeEditor
{
public:
    const char* GetTypeName() const override { return "ParticleGraph"; }
    const char* GetDisplayName() const override { return "Particle Effect"; }
    const char* GetExtension() const override { return ".asset"; }

    // A new effect starts as the Emitter node wired to an Emission node, the
    // smallest graph that produces particles. Without emission a new effect
    // would spawn nothing and look broken.
    const char* GetDefaultContent() const override
    {
        return R"({
  "links": [
    { "from": 1, "fromPin": 0, "to": 2, "toPin": 0 }
  ],
  "nextNodeId": 3,
  "nodes": [
    { "id": 1, "type": "ParticleEmit", "values": {}, "x": 60.0, "y": 120.0 },
    { "id": 2, "type": "ParticleEmission", "values": {}, "x": 300.0, "y": 120.0 }
  ],
  "type": "ParticleGraph"
})";
    }

    int GetCompileTarget() const override { return 2; }  // Data
};

REGISTER_EDITOR(ParticleGraphAssetEditor)

}  // namespace DekiEditor

// Implemented in ParticlePreview.cpp: runs the graph being edited and draws
// its particles, so the Node Graph window can offer a Preview panel.
DekiNodeGraph::NodeGraphPreviewOps DekiParticlesPreviewOps();

// Implemented in ParticleNodeGizmos.cpp: draws the selected node's shape, arc
// or ramp in the properties panel, under its title.
DekiNodeGraph::NodeGraphNodeGizmoOps DekiParticlesGizmoOps();

REGISTER_NODE_GRAPH_DOMAIN_PREVIEW_GIZMOS(kParticleDomain, "ParticleGraph", "Particle Effect", "Particles",
                                          "ParticleEmit", DekiParticlesPreviewOps(), DekiParticlesGizmoOps());

using namespace DekiEditor;

// Registers the domain again after a plugin-only hot reload, which clears the
// domain registry while this DLL stays loaded, so the static registrar above
// does not run again. Register() skips duplicates, so repeating is safe.
// Called from DekiParticlesRegisterGraphTypes (DekiParticlesPackage.cpp).
extern "C" void DekiParticlesRegisterEditorGraphDomain(void)
{
    DekiNodeGraph::NodeGraphDomainRegistry::Instance().Register(&kParticleDomain);
}

#endif  // DEKI_EDITOR
