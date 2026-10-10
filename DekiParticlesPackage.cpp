// Entry point of the deki-particles DLL. Exports the standard Deki plugin
// interface, so the editor can load it and register its one component (the
// emitter) and its particle-graph nodes: the Emitter entry and the built-in
// modifiers (Emission, Initial Velocity, Initial Rotation, Gravity, Drag, and
// Size, Color and Rotation over Lifetime).
//
// Other packages add modifiers the same way: declare a DEKI_NODE struct in a
// category starting "Particles/", register its ParticleModifierOps with
// REGISTER_PARTICLE_MODIFIER, and register the node type from their own
// package entry. deki-particles needs no change.

#include <deki/interop/Plugin.h>
#include "ParticleEmitterComponent.h"
#include "ParticleNodes.h"
#include "ParticleSystem.h"
#include "ParticlesInit.h"
#include <deki/reflection/ComponentRegistry.h>
#include <deki/reflection/ComponentFactory.h>
#include "deki-nodegraph/DekiNode.h"  // DekiNodeGraph::NodeFactory + DekiNodeGraph::NodeTypeRegistry (editor)

extern void DekiParticlesRegisterComponents();
extern int DekiParticlesGetAutoComponentCount();
extern const Deki::ComponentMeta* DekiParticlesGetAutoComponentMeta(int index);

namespace DekiParticles
{

#ifdef DEKI_EDITOR

// The exports below are C symbols at global scope; the package's own
// registration helpers and statics live in its namespace.
using namespace DekiParticles;

// Defined in editor/ParticleGraphEditor.cpp; registers the graph domain again.
extern "C" void DekiParticlesRegisterEditorGraphDomain(void);

static bool s_ParticlesRegistered = false;

namespace
{
// Does what the generated REGISTER_RUNTIME_NODE/REGISTER_NODE static
// registrars do, but can run again. Those run once at DLL load, while the
// editor's plugin-only hot reload clears the shared node registries without
// unloading this package. DekiNodeGraph::NodeFactory overwrites by typeId and
// DekiNodeGraph::NodeTypeRegistry skips duplicates, so repeating is safe.
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
    /// Registers this package's node graph types: modifier node factories,
    /// editor metas and the Particles graph domain. Called at package load via
    /// ::DekiPluginRegisterComponents, and again after any registry clear that
    /// keeps this DLL loaded (plugin-only hot reload).
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
        DekiParticlesInitSystem();
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
        // Outside the s_ParticlesRegistered latch on purpose: every hot reload
        // (full or plugin-only) clears the node registries, and this export is
        // how they are filled again in the plugin-only case.
        DekiParticlesRegisterGraphTypes();
    }

    DEKI_PLUGIN_API void DekiPluginOnPlayModeStop(void)
    {
        ParticleSystem::GetInstance().ClearAll();
    }

    // Package-specific API, named so linked DLLs do not clash.
    DEKI_PARTICLES_API const char* DekiParticlesGetName(void)
    {
        return "Particles";
    }
}  // extern "C"

#endif  // DEKI_EDITOR
}  // namespace DekiParticles
