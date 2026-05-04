/**
 * @file DekiParticlesModule.cpp
 * @brief Module entry point for deki-particles DLL
 *
 * Exports the standard Deki plugin interface so the editor can load
 * deki-particles.dll and register its components — the emitter plus all
 * built-in modifiers (Emission, Shape, InitialVelocity, InitialRotation,
 * Gravity, Drag, RotationOverLifetime, SizeOverLifetime, ColorOverLifetime).
 *
 * External particle modules ship the same way: declare a DEKI_COMPONENT
 * subclass of ParticleModifier, list it in your own module.json features,
 * and the emitter discovers it automatically via dynamic_cast on its
 * sibling components — no changes to deki-particles required.
 */

#include "interop/DekiPlugin.h"
#include "DekiModuleFeatureMeta.h"
#include "ParticleEmitterComponent.h"
#include "ParticleModifier.h"
#include "EmissionModifier.h"
#include "InitialVelocityModifier.h"
#include "InitialRotationModifier.h"
#include "GravityModifier.h"
#include "DragModifier.h"
#include "RotationOverLifetimeModifier.h"
#include "SizeOverLifetimeModifier.h"
#include "ColorOverLifetimeModifier.h"
#include "ParticleSystem.h"
#include "reflection/ComponentRegistry.h"
#include "reflection/ComponentFactory.h"
#ifdef DEKI_EDITOR
#include "imgui.h"
#endif

#ifdef DEKI_EDITOR

extern void DekiParticles_RegisterComponents();
extern int  DekiParticles_GetAutoComponentCount();
extern const DekiComponentMeta* DekiParticles_GetAutoComponentMeta(int index);

static bool s_ParticlesRegistered = false;

extern "C" {

DEKI_PARTICLES_API int DekiParticles_EnsureRegistered(void)
{
    if (s_ParticlesRegistered)
        return DekiParticles_GetAutoComponentCount();
    s_ParticlesRegistered = true;
    DekiParticles_RegisterComponents();
    return DekiParticles_GetAutoComponentCount();
}

DEKI_PLUGIN_API const char* DekiPlugin_GetName(void)
{
    return "Deki Particles Module";
}

DEKI_PLUGIN_API const char* DekiPlugin_GetVersion(void)
{
#ifdef DEKI_MODULE_VERSION
    return DEKI_MODULE_VERSION;
#else
    return "0.0.0-dev";
#endif
}

DEKI_PLUGIN_API const char* DekiPlugin_GetReflectionJson(void)
{
    return "{}";
}

DEKI_PLUGIN_API int DekiPlugin_Init(void)
{
    return 0;
}

DEKI_PLUGIN_API void DekiPlugin_Shutdown(void)
{
    s_ParticlesRegistered = false;
    ParticleSystem::GetInstance().ClearAll();
}

DEKI_PLUGIN_API int DekiPlugin_GetComponentCount(void)
{
    return DekiParticles_GetAutoComponentCount();
}

DEKI_PLUGIN_API const DekiComponentMeta* DekiPlugin_GetComponentMeta(int index)
{
    return DekiParticles_GetAutoComponentMeta(index);
}

DEKI_PLUGIN_API void DekiPlugin_RegisterComponents(void)
{
    DekiParticles_EnsureRegistered();
}

DEKI_PLUGIN_API void DekiPlugin_OnPlayModeStop(void)
{
    ParticleSystem::GetInstance().ClearAll();
}

#ifdef DEKI_EDITOR
DEKI_PLUGIN_API void DekiPlugin_SetImGuiContext(void* ctx)
{
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));
}
#endif

// =============================================================================
// Module Feature API
// =============================================================================

static const char* s_ParticleGuids[] = {
    ParticleEmitterComponent::StaticGuid,
    EmissionModifier::StaticGuid,
    InitialVelocityModifier::StaticGuid,
    InitialRotationModifier::StaticGuid,
    GravityModifier::StaticGuid,
    DragModifier::StaticGuid,
    RotationOverLifetimeModifier::StaticGuid,
    SizeOverLifetimeModifier::StaticGuid,
    ColorOverLifetimeModifier::StaticGuid,
};

static const DekiModuleFeatureInfo s_Features[] = {
    {
        "particle-emitter", "Particle Emitter",
        "Modular particle system: emitter plus built-in behavior modifiers",
        true, "DEKI_FEATURE_PARTICLE_EMITTER",
        s_ParticleGuids,
        sizeof(s_ParticleGuids) / sizeof(s_ParticleGuids[0])
    },
};

DEKI_PLUGIN_API int DekiPlugin_GetFeatureCount(void)
{
    return sizeof(s_Features) / sizeof(s_Features[0]);
}

DEKI_PLUGIN_API const DekiModuleFeatureInfo* DekiPlugin_GetFeature(int index)
{
    if (index < 0 || index >= DekiPlugin_GetFeatureCount())
        return nullptr;
    return &s_Features[index];
}

// Module-specific feature API (for linked-DLL access without name conflicts)
DEKI_PARTICLES_API const char* DekiParticles_GetName(void)        { return "Particles"; }
DEKI_PARTICLES_API int DekiParticles_GetFeatureCount(void)        { return DekiPlugin_GetFeatureCount(); }
DEKI_PARTICLES_API const DekiModuleFeatureInfo* DekiParticles_GetFeature(int index) { return DekiPlugin_GetFeature(index); }

} // extern "C"

#endif // DEKI_EDITOR
