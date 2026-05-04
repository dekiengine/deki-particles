#pragma once

#include "DekiParticlesAPI.h"
#include "deki-rendering/RendererComponent.h"
#include "deki-2d/Sprite.h"
#include "assets/AssetRef.h"
#include "reflection/DekiProperty.h"
#include "ParticlePool.h"
#include "ParticleMath.h"
#include <vector>

class ParticleModifier;

/**
 * @brief Renderable particle emitter.
 *
 * Owns a fixed-size particle pool. All authoring (emission rate, spawn shape,
 * gravity, color/size/rotation curves, etc.) is delegated to sibling
 * ParticleModifier components attached to the same DekiObject.
 *
 * Render path: rasterizes all alive particles into a single intermediate
 * RGB565A8 buffer sized to the tight screen-space bounding box and returns
 * one QuadBlit::Source. The framework does the final blit at the emitter's
 * world transform — sort order is per-emitter, never per-particle.
 *
 * Per CLAUDE.md "NEVER USE FALLBACKS": no sprite ⇒ renders nothing, logs
 * once. No EmissionModifier sibling ⇒ no particles ever spawn. maxParticles
 * is honored exactly.
 */
class DEKI_PARTICLES_API ParticleEmitterComponent : public RendererComponent
{
public:
    DEKI_COMPONENT(ParticleEmitterComponent, RendererComponent, "Particles", "b1e0e1a0-1111-4002-9002-000000000010", "DEKI_FEATURE_PARTICLE_EMITTER")

    DEKI_EXPORT
    Deki::AssetRef<Sprite> sprite;

    DEKI_EXPORT
    DEKI_RANGE(0, 4096)
    int32_t maxParticles = 64;

    DEKI_EXPORT
    bool playOnAwake = true;

    DEKI_EXPORT
    bool looping = true;

    DEKI_EXPORT
    bool worldSpace = true;

    ParticleEmitterComponent();
    virtual ~ParticleEmitterComponent();

    void Awake() override;
    void Start() override;
    void Update() override;
    void UnloadAssets() override;

    bool RenderContent(const DekiObject* owner,
                       QuadBlit::Source& outSource,
                       float& outPivotX,
                       float& outPivotY,
                       uint8_t& outTintR,
                       uint8_t& outTintG,
                       uint8_t& outTintB,
                       uint8_t& outTintA) override;

    // Public so modifiers can read/write directly. Hot-path inner loops touch
    // these without going through accessors.
    deki_particles::ParticlePool pool;
    deki_particles::Xorshift32   rng;

    // Cached modifier hook lists (populated in Start, refreshable).
    void RefreshModifiers();
    const std::vector<ParticleModifier*>& EmitModifiers() const { return m_OnEmit; }
    const std::vector<ParticleModifier*>& SimulateModifiers() const { return m_OnSimulate; }

    // Spawn one particle. Returns its index in [0, AliveCount), or -1 if full.
    // Calls OnEmit on every modifier in phase order (including the modifier
    // that called Spawn — modifiers above its phase will not see this
    // particle until next frame, which is intentional and consistent).
    int  Spawn();

    // Single tick of simulation: age, kill, dispatch modifiers, integrate.
    // Update() calls this with the engine's frame delta. The editor preview
    // path also calls it (with the editor's frame delta) so emitters animate
    // in edit mode without needing Play.
    void Simulate(float dt);

#ifdef DEKI_EDITOR
    // Editor-only preview controls. State is NOT serialized.
    bool IsEditorPreviewPlaying() const { return m_EditorPreviewPlaying; }
    void EditorPreviewSetPlaying(bool play) { m_EditorPreviewPlaying = play; }
    // Kill all live particles and reset every modifier (re-runs OnAttachToEmitter
    // so EmissionModifier's first-burst flag, rate accumulator, etc. all reset).
    void EditorPreviewRestart();
#endif

    // Pool allocation + modifier discovery + per-modifier OnAttachToEmitter
    // bootstrap, factored out of Start() so the editor preview path can run
    // it in edit mode (where Start() never fires). Idempotent.
    void EnsureReady();

private:
    bool m_PoolAllocated = false;
    bool m_ModifiersAttached = false;
    bool m_LoggedMissingSprite = false;
#ifdef DEKI_EDITOR
    bool m_EditorPreviewPlaying = false;
#endif

    // Render-side persistent buffer (grows to fit, never shrinks for jitter).
    uint8_t* m_BboxBuf = nullptr;
    int      m_BboxBufBytes = 0;

    // Cached, phase-sorted modifier hook lists.
    std::vector<ParticleModifier*> m_OnEmit;
    std::vector<ParticleModifier*> m_OnSimulate;

    void EnsurePoolAllocated();
    void FreeBboxBuf();
};

// Generated property metadata
#include "generated/ParticleEmitterComponent.gen.h"
