#pragma once

#include "DekiParticlesAPI.h"
#include "deki-rendering/RendererComponent.h"
#include "deki-2d/Sprite.h"
#include <deki/assets/AssetRef.h>
#include <deki/reflection/Property.h>
#include "ParticlePool.h"
#include "ParticleMath.h"
#include "ParticleGraph.h"
#include "ParticleChain.h"
#include <vector>

namespace DekiParticles
{

/// A particle emitter that draws itself.
///
/// It owns a fixed-size particle pool. All authoring (emission rate, spawn
/// shape, gravity, colour, size and rotation curves, etc.) lives in a
/// ParticleGraph asset: an Emitter node followed by a chain of modifier nodes,
/// wired in the order they run. The emitter walks that graph once
/// (EnsureReady) into a flat list of callbacks, so per particle the cost is
/// exactly the modifiers wired, with no graph interpretation in the loop.
///
/// One graph drives any number of emitters. The node instances holding the
/// tuning values belong to the asset and are shared; each emitter owns only
/// the small per-modifier state blobs its chain allocated.
///
/// Units: particle positions and velocities are world metres, like every other
/// component (the graph nodes' speeds are m/s). The sprite is drawn at its own
/// pixelsPerMeter, so when the camera runs at the sprite's ppm a particle has
/// the size its art was authored at, and scale 1 means "as authored".
///
/// Rendering: all live particles are drawn into one RGB565A8 buffer at the
/// sprite's pixel scale, sized to their tight bounding box, and returned as one
/// QuadBlit::Source with the sprite's pixelsPerMeter. The renderer blits it at
/// the emitter's world transform, so sort order is per emitter, never per
/// particle. GetContentExtents reports the same box, so an emitter whose
/// particles are all off screen or clipped away does no work.
///
/// No fallbacks: with no sprite it draws nothing and logs once; with no graph
/// no particles spawn; a graph naming a modifier type with no registered
/// behaviour refuses to build its chain and logs it. maxParticles is honoured
/// exactly.
DEKI_CATEGORY("Particles")
DEKI_DESCRIPTION("Spawns and draws particles, following a particle graph asset.")
class DEKI_PARTICLES_API ParticleEmitterComponent : public DekiRendering::RendererComponent
{
public:
    DEKI_EXPORT
    DEKI_TOOLTIP(
        "Image drawn for each particle. A small, soft sprite hides the low particle counts a device can afford.")
    Deki::AssetRef<Deki2D::Sprite> sprite;

    // The effect recipe. Assign a ".asset" of type "ParticleGraph", authored
    // in the Node Graph window. No graph means no chain and no particles.
    DEKI_EXPORT
    DEKI_TOOLTIP(
        "The particle graph asset, which is where emission, velocity, size and colour over lifetime are authored.")
    Deki::AssetRef<ParticleGraph> graph;

    DEKI_EXPORT
    DEKI_TOOLTIP("Ceiling on particles alive at once. The pool is allocated once at this size, so it is a memory "
                 "decision as much as a visual one.")
    DEKI_RANGE(0, 4096)
    int32_t maxParticles = 64;

    DEKI_EXPORT
    DEKI_TOOLTIP("Start emitting as soon as the object comes alive. Off, something has to start it.")
    bool playOnAwake = true;

    DEKI_EXPORT
    DEKI_TOOLTIP("Restart the effect when it finishes instead of stopping. Off, it plays once and goes quiet.")
    bool looping = true;

    DEKI_EXPORT
    DEKI_TOOLTIP("Leave particles where they were born when the emitter moves. Off, they travel with it, which suits a "
                 "flame carried by a character and not smoke left behind one.")
    bool worldSpace = true;

    ParticleEmitterComponent();
    virtual ~ParticleEmitterComponent();

    void Awake() override;
    void Start() override;
    void Update() override;
    void UnloadAssets() override;

    bool GetContentExtents(float& outWidth, float& outHeight) const override;

    bool RenderContent(const Deki::Object* owner, QuadBlit::Source& outSource, float& outPivotX, float& outPivotY,
                       uint8_t& outTintR, uint8_t& outTintG, uint8_t& outTintB, uint8_t& outTintA) override;

    // Public so modifiers' inner loops can use them directly, without accessors.
    DekiParticles::ParticlePool pool;
    DekiParticles::Xorshift32 rng;

    /// Walks the graph asset into m_Chain (see ParticleChain.h). EnsureReady
    /// calls it; call it yourself after assigning a different graph asset.
    /// Returns false, with the chain empty, when there is no graph to walk yet.
    bool RebuildChain();

    const std::vector<ParticleChainEntry>& Chain() const { return m_Chain; }

#ifdef DEKI_EDITOR
    /// Takes a chain built elsewhere, with ownership of its state blobs, and
    /// runs its attach pass. The editor preview uses it for the graph being
    /// edited, which has no asset yet.
    void AdoptChain(std::vector<ParticleChainEntry>&& chain);
#endif

    /// Spawns one particle. Returns its index in [0, AliveCount), or -1 if
    /// full. Calls OnEmit on every modifier in phase order, including the one
    /// that called Spawn. Modifiers before that one in the chain first see the
    /// particle next frame, by design.
    int Spawn();

    /// One simulation step: age, kill, run the modifiers, integrate. Update()
    /// calls it with the engine's frame delta; the editor preview calls it with
    /// the editor's, so emitters animate in edit mode without Play.
    void Simulate(float dt);

#ifdef DEKI_EDITOR
    // Editor-only preview controls. Their state is not saved.
    bool IsEditorPreviewPlaying() const { return m_EditorPreviewPlaying; }
    void EditorPreviewSetPlaying(bool play) { m_EditorPreviewPlaying = play; }
    /// Kills all live particles and rebuilds the chain from the graph, which
    /// resets every modifier's state (the burst latch, the rate accumulator).
    void EditorPreviewRestart();
#endif

    /// Allocates the pool, builds the chain and runs each modifier's onAttach.
    /// Separate from Start() so the editor preview can run it in edit mode,
    /// where Start() never fires. Safe to call repeatedly.
    void EnsureReady();

private:
    bool m_PoolAllocated = false;
    bool m_ChainAttached = false;
    bool m_LoggedMissingSprite = false;
    bool m_LoggedBadGraph = false;
#ifdef DEKI_EDITOR
    bool m_EditorPreviewPlaying = false;
#endif

    // Composite buffer kept between frames. It grows to fit and never shrinks,
    // so a size that jitters does not reallocate.
    uint8_t* m_BboxBuf = nullptr;
    int m_BboxBufBytes = 0;

    // The built chain, in wire order.
    std::vector<ParticleChainEntry> m_Chain;

    void EnsurePoolAllocated();
    void FreeBboxBuf();
    void FreeChain();

    // Tight bounding box of the live particles in composite pixels (the
    // sprite's pixel scale), relative to the emitter origin. False when there
    // is nothing to draw.
    struct Bounds
    {
        int32_t minX, minY, maxX, maxY;
        float ppm;  // composite pixels per world metre (the sprite's)
    };
    bool ComputeBounds(const Deki2D::Sprite* spr, float anchorX, float anchorY, Bounds& out) const;
    void AnchorFor(const Deki::Object* owner, float& anchorX, float& anchorY) const;
};

}  // namespace DekiParticles
