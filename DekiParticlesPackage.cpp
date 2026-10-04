/**
 * @file DekiParticlesPackage.cpp
 * @brief Package entry point for deki-particles DLL
 *
 * Exports the standard Deki plugin interface so the editor can load
 * deki-particles.dll and register its one component (the emitter) plus the
 * particle-graph node vocabulary: the Emitter entry and the built-in modifier
 * nodes (Emission, Initial Velocity, Initial Rotation, Gravity, Drag, and the
 * Size / Color / Rotation over Lifetime trio).
 *
 * External particle packages ship modifiers the same way: declare a DEKI_NODE
 * struct in a category starting "Particles/", register its ParticleModifierOps
 * with REGISTER_PARTICLE_MODIFIER, and register the node type from your own
 * package entry. No changes to deki-particles required.
 */

#include <deki/interop/Plugin.h>
#include "ParticleEmitterComponent.h"
#include "ParticleNodes.h"
#include "ParticleSystem.h"
#include <deki/reflection/ComponentRegistry.h>
#include <deki/reflection/ComponentFactory.h>
#include "deki-nodegraph/DekiNode.h"  // DekiNodeGraph::NodeFactory + DekiNodeGraph::NodeTypeRegistry (editor)

extern void DekiParticlesRegisterComponents();
extern int DekiParticlesGetAutoComponentCount();
extern const Deki::ComponentMeta* DekiParticlesGetAutoComponentMeta(int index);

namespace DekiParticles
{

#ifdef DEKI_EDITOR

// Defined in editor/ParticleGraphEditor.cpp (re-registers the graph domain).

// The exports below are C symbols at global scope; the package's own
// registration helpers and statics live in its namespace.
using namespace DekiParticles;

extern "C" void DekiParticlesRegisterEditorGraphDomain(void);

static bool s_ParticlesRegistered = false;

namespace
{
// Re-runnable mirror of the generated REGISTER_RUNTIME_NODE/REGISTER_NODE
// static registrars. Those run once at DLL load; the editor's plugin-only
// hot reload wipes the shared node registries WITHOUT unloading this
// package, so registration must be repeatable on demand. DekiNodeGraph::NodeFactory
// overwrites by typeId and DekiNodeGraph::NodeTypeRegistry dedupes, so this is idempotent.
template <typename T>
void RegisterParticleNodeType()
{
    DekiNodeGraph::SceneFormat::NodeFactory::Instance().Register(
        Deki::HashString(T::StaticNodeName), []() -> void* { return new T(); },
        [](void* p, Deki::SceneFormat::SceneMsgPackParser& parser, uint32_t mapSize) -> bool
        { return DeserializeMsgPack(*static_cast<T*>(p), parser, mapSize); },
        [](void* p) { delete static_cast<T*>(p); });
    DekiNodeGraph::NodeTypeRegistry::Instance().Register(&T::GetNodeMeta(), sizeof(DekiNodeGraph::DekiNodeMeta));
}
}  // namespace

extern "C"
{
    /**
     * @brief (Re-)register this package's node graph types: modifier node
     * factories, editor metas, and the Particles graph domain. Called at package
     * load via ::DekiPluginRegisterComponents and again after any registry wipe
     * that keeps this DLL loaded (plugin-only hot reload).
     */
    DEKI_PARTICLES_API void DekiParticlesRegisterGraphTypes(void)
    {
        RegisterParticleNodeType<ParticleEmitNode>();
        RegisterParticleNodeType<ParticleEmissionNode>();
        RegisterParticleNodeType<ParticleInitialVelocityNode>();
        RegisterParticleNodeType<ParticleInitialRotationNode>();
        RegisterParticleNodeType<ParticleGravityNode>();
        RegisterParticleNodeType<ParticleDragNode>();
        RegisterParticleNodeType<ParticleSizeOverLifetimeNode>();
        RegisterParticleNodeType<ParticleColorOverLifetimeNode>();
        RegisterParticleNodeType<ParticleRotationOverLifetimeNode>();
        DekiParticlesRegisterEditorGraphDomain();
    }

    DEKI_PARTICLES_API int DekiParticlesEnsureRegistered(void)
    {
        if (s_ParticlesRegistered)
        {
            return ::DekiParticlesGetAutoComponentCount();
        }
        s_ParticlesRegistered = true;
        ::DekiParticlesRegisterComponents();
        return ::DekiParticlesGetAutoComponentCount();
    }

    DEKI_PLUGIN_API const char* DekiPluginGetName(void)
    {
        return "Deki Particles Package";
    }

    DEKI_PLUGIN_API const char* DekiPluginGetVersion(void)
    {
#ifdef DEKI_PACKAGE_VERSION
        return DEKI_PACKAGE_VERSION;
#else
        return "0.0.0-dev";
#endif
    }

    DEKI_PLUGIN_API int DekiPluginInit(void)
    {
        return 0;
    }

    DEKI_PLUGIN_API void DekiPluginShutdown(void)
    {
        s_ParticlesRegistered = false;
        ParticleSystem::GetInstance().ClearAll();
    }

    DEKI_PLUGIN_API int DekiPluginGetComponentCount(void)
    {
        return ::DekiParticlesGetAutoComponentCount();
    }

    DEKI_PLUGIN_API const Deki::ComponentMeta* DekiPluginGetComponentMeta(int index)
    {
        return ::DekiParticlesGetAutoComponentMeta(index);
    }

    DEKI_PLUGIN_API void DekiPluginRegisterComponents(void)
    {
        DekiParticlesEnsureRegistered();
        // Deliberately OUTSIDE the s_ParticlesRegistered latch: node registries
        // are wiped on every hot reload (full or plugin-only) and this export is
        // the re-registration path for the plugin-only case.
        DekiParticlesRegisterGraphTypes();
    }

    DEKI_PLUGIN_API void DekiPluginOnPlayModeStop(void)
    {
        ParticleSystem::GetInstance().ClearAll();
    }

    // Package-specific feature API (for linked-DLL access without name conflicts)
    DEKI_PARTICLES_API const char* DekiParticlesGetName(void)
    {
        return "Particles";
    }
}  // extern "C"

#endif  // DEKI_EDITOR
}  // namespace DekiParticles
