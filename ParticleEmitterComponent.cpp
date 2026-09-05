#include "ParticleEmitterComponent.h"
#include "PixelFormat.h"
#include "ParticleNodes.h"
#include "ParticleSystem.h"
#include "DekiObject.h"
#include "DekiTime.h"
#include "DekiLogSystem.h"
#include "deki-2d/Texture2D.h"
#include "deki-rendering/QuadBlit.h"
#include "DekiEngine.h"  // for DekiColorFormat enum
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>

ParticleEmitterComponent::ParticleEmitterComponent()
{
    SetNeedsUpdate(true);
    rng.Seed((uint32_t)(uintptr_t)this);
}

ParticleEmitterComponent::~ParticleEmitterComponent()
{
    ParticleSystem::GetInstance().UnregisterEmitter(this);
    FreeBboxBuf();
    FreeChain();
}

void ParticleEmitterComponent::Awake()
{
    ParticleSystem::GetInstance().RegisterEmitter(this);
}

void ParticleEmitterComponent::Start()
{
    EnsureReady();
}

void ParticleEmitterComponent::EnsureReady()
{
    EnsurePoolAllocated();
    if (m_ChainAttached) return;

    if (!RebuildChain())
        return;  // No graph loaded yet — try again next tick.

    // Give every modifier a chance to allocate its private state and request
    // pool extension columns now that capacity is known.
    for (const ParticleChainEntry& e : m_Chain)
        if (e.ops->onAttach) e.ops->onAttach(e.data, e.state, *this);
    m_ChainAttached = true;
}

void ParticleEmitterComponent::EnsurePoolAllocated()
{
    if (m_PoolAllocated && pool.Capacity() == maxParticles)
        return;
    pool.SetCapacity(maxParticles);
    m_PoolAllocated = true;
}

void ParticleEmitterComponent::FreeChain()
{
    FreeParticleChain(m_Chain);
}

bool ParticleEmitterComponent::RebuildChain()
{
    FreeChain();

    ParticleGraph* g = graph.Get();
    if (!g || !g->data)
        return false;   // Not loaded yet. Not an error: assets resolve later.

    const char* error = nullptr;
    if (BuildParticleChain(g->data->Root(),
                           Deki::HashString(ParticleEmitNode::StaticNodeName),
                           m_Chain, &error))
        return true;

    if (error && !m_LoggedBadGraph)
    {
        DEKI_LOG_ERROR("ParticleEmitterComponent: %s", error);
        m_LoggedBadGraph = true;
    }
    return false;
}

#ifdef DEKI_EDITOR
void ParticleEmitterComponent::AdoptChain(std::vector<ParticleChainEntry>&& chain)
{
    FreeChain();
    m_Chain = std::move(chain);
    EnsurePoolAllocated();
    for (const ParticleChainEntry& e : m_Chain)
        if (e.ops->onAttach) e.ops->onAttach(e.data, e.state, *this);
    m_ChainAttached = true;
}
#endif

int ParticleEmitterComponent::Spawn()
{
    int idx = pool.Spawn();
    if (idx < 0) return -1;

    // Reset particle state for this slot. All numeric state is float.
    pool.posX[idx] = 0.0f;
    pool.posY[idx] = 0.0f;
    pool.velX[idx] = 0.0f;
    pool.velY[idx] = 0.0f;
    pool.age[idx] = 0.0f;
    pool.lifetime[idx] = 1.0f;
    if (pool.HasRotation())      { pool.rotation[idx] = 0.0f; pool.rotationSpeed[idx] = 0.0f; }
    if (pool.HasScale())         { pool.scale[idx] = 1.0f; }
    if (pool.HasTint())          { pool.tintR[idx] = 255; pool.tintG[idx] = 255; pool.tintB[idx] = 255; pool.tintA[idx] = 255; }

    // Drive onEmit through every ENABLED modifier in chain order. The
    // Emission node is the typical caller; the spawn-time setters wired after
    // it read the lifetime it set and write initial pos/vel/rot.
    for (const ParticleChainEntry& e : m_Chain)
    {
        if (!e.ops->onEmit) continue;
        if (e.ops->isEnabled && !e.ops->isEnabled(e.data)) continue;
        e.ops->onEmit(e.data, e.state, *this, idx);
    }

    return idx;
}

void ParticleEmitterComponent::Update()
{
    float dt = Deki::Time::GetDeltaTimeF() / 1000.0f;
    Simulate(dt);
}

void ParticleEmitterComponent::Simulate(float dt)
{
    // Self-bootstrap so the editor preview path works without Start() ever
    // having fired (Play mode is the only context where the engine drives
    // the lifecycle).
    EnsureReady();

    if (m_Chain.empty())
        return;

    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;  // Clamp huge deltas (loading, paused, etc.) to avoid teleport.

    // Age + kill expired particles BEFORE driving simulation hooks. This way
    // OnSimulate iterates only currently-alive particles. age/lifetime are
    // float; convert dt once before the loop.
    float dtN = static_cast<float>(dt);
    int n = pool.AliveCount();
    for (int i = 0; i < n; )
    {
        pool.age[i] = ((pool.age[i]) + (dtN));
        if (pool.age[i] >= pool.lifetime[i])
        {
            pool.KillSwap(i);
            n--;
            continue;
        }
        ++i;
    }

    for (const ParticleChainEntry& e : m_Chain)
    {
        if (!e.ops->onSimulate) continue;
        if (e.ops->isEnabled && !e.ops->isEnabled(e.data)) continue;
        e.ops->onSimulate(e.data, e.state, *this, dt);
    }

    // Final kinematic integration (pos += vel * dt) — runs once after all
    // force modifiers have mutated velocity. Always happens, even with no
    // modifiers attached, so that a programmatic Spawn() with non-zero
    // velocity still moves.
    int alive = pool.AliveCount();
    float* px = pool.posX; float* py = pool.posY;
    float* vx = pool.velX; float* vy = pool.velY;
    // dtN was already converted from `dt` earlier in this function for the
    // age-aging loop — reuse it here instead of redefining.
    for (int i = 0; i < alive; ++i)
    {
        px[i] = ((px[i]) + (((vx[i]) * (dtN))));
        py[i] = ((py[i]) + (((vy[i]) * (dtN))));
    }
}

#ifdef DEKI_EDITOR
void ParticleEmitterComponent::EditorPreviewRestart()
{
    // Wipe live particles. We don't free the pool — capacity stays so the
    // user's previewed particle count survives across restarts.
    if (m_PoolAllocated)
    {
        // Drain to zero alive without touching column pointers.
        while (pool.AliveCount() > 0)
            pool.KillSwap(pool.AliveCount() - 1);
    }
    // Rebuild the chain from the currently loaded graph, so every modifier's
    // state blob starts over (the burst latch, the rate accumulator) and a
    // reimported graph asset takes effect. Clearing the attach flag makes
    // EnsureReady re-run the whole pass.
    m_ChainAttached = false;
    m_LoggedBadGraph = false;
    EnsureReady();
}
#endif

void ParticleEmitterComponent::UnloadAssets()
{
    sprite.ptr = nullptr;
    sprite.loadAttempted = false;

    // The chain points INTO the graph asset's node instances, so it cannot
    // outlive the asset. Drop it before releasing the reference.
    FreeChain();
    m_ChainAttached = false;
    graph.ptr = nullptr;
    graph.loadAttempted = false;

    FreeBboxBuf();
    m_LoggedMissingSprite = false;
    m_LoggedBadGraph = false;
}

void ParticleEmitterComponent::FreeBboxBuf()
{
    delete[] m_BboxBuf;
    m_BboxBuf = nullptr;
    m_BboxBufBytes = 0;
}

namespace
{
// Composite pixels per world metre: the sprite's own scale, so the emitter's
// output is 1:1 with the art whenever the camera runs at the sprite's ppm.
inline float SpritePpm(const Sprite* spr)
{
    return (spr && spr->pixelsPerMeter > 0.0f) ? spr->pixelsPerMeter : 1.0f;
}
}  // namespace

void ParticleEmitterComponent::AnchorFor(const Deki::Object* owner, float& anchorX, float& anchorY) const
{
    // worldSpace=true: particles store world coords; subtract the emitter's
    // world origin so the composite sits at the emitter after the final
    // transform. worldSpace=false: particles are emitter-local already.
    anchorX = 0.0f;
    anchorY = 0.0f;
    if (worldSpace && owner)
    {
        anchorX = owner->GetWorldX();
        anchorY = owner->GetWorldY();
    }
}

bool ParticleEmitterComponent::ComputeBounds(const Sprite* spr, float anchorX, float anchorY, Bounds& out) const
{
    const int n = pool.AliveCount();
    if (n <= 0 || !spr)
        return false;

    const float ppm = SpritePpm(spr);
    const float spriteW = static_cast<float>(spr->width);
    const float spriteH = static_cast<float>(spr->height);
    // Rotation expands the box up to sqrt(2). 1.45 leaves a one-pixel guard band.
    constexpr float kRotPad = 1.45f;
    const bool hasScale = pool.HasScale();
    const float* px = pool.posX;
    const float* py = pool.posY;
    const float* ps = pool.scale;

    float minX = 1e9f, minY = 1e9f;
    float maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i)
    {
        const float lx = (px[i] - anchorX) * ppm;  // metres -> composite pixels
        const float ly = (py[i] - anchorY) * ppm;
        const float s = hasScale ? ps[i] : 1.0f;
        const float halfW = 0.5f * spriteW * s * kRotPad;
        const float halfH = 0.5f * spriteH * s * kRotPad;
        if (lx - halfW < minX) minX = lx - halfW;
        if (ly - halfH < minY) minY = ly - halfH;
        if (lx + halfW > maxX) maxX = lx + halfW;
        if (ly + halfH > maxY) maxY = ly + halfH;
    }

    out.minX = static_cast<int32_t>(std::floor(minX));
    out.minY = static_cast<int32_t>(std::floor(minY));
    out.maxX = static_cast<int32_t>(std::ceil(maxX));
    out.maxY = static_cast<int32_t>(std::ceil(maxY));
    out.ppm = ppm;
    return (out.maxX - out.minX) > 0 && (out.maxY - out.minY) > 0;
}

bool ParticleEmitterComponent::GetContentExtents(float& outWidth, float& outHeight) const
{
    const Sprite* spr = sprite.Get();
    if (!spr || !spr->data)
        return false;  // RenderContent logs the missing sprite; let it run

    if (pool.AliveCount() <= 0)
    {
        outWidth = outHeight = 0.0f;  // nothing to draw: cull the RenderContent call too
        return true;
    }

    float anchorX, anchorY;
    AnchorFor(GetOwner(), anchorX, anchorY);
    Bounds b;
    if (!ComputeBounds(spr, anchorX, anchorY, b))
    {
        outWidth = outHeight = 0.0f;
        return true;
    }
    // The renderer assumes the content lies within one full extent of the
    // origin in every direction; the box is not centred on the emitter, so
    // report the farther edge on each axis.
    const float reachX = static_cast<float>(std::max(std::abs(b.minX), std::abs(b.maxX)));
    const float reachY = static_cast<float>(std::max(std::abs(b.minY), std::abs(b.maxY)));
    outWidth = reachX / b.ppm;
    outHeight = reachY / b.ppm;
    return true;
}

bool ParticleEmitterComponent::RenderContent(const Deki::Object* owner,
                                              QuadBlit::Source& outSource,
                                              float& outPivotX,
                                              float& outPivotY,
                                              uint8_t& outTintR,
                                              uint8_t& outTintG,
                                              uint8_t& outTintB,
                                              uint8_t& outTintA)
{
    outTintR = outTintG = outTintB = outTintA = 255;

    Sprite* spr = sprite.Get();
    if (!spr || !spr->data)
    {
        if (!m_LoggedMissingSprite)
        {
            DEKI_LOG_ERROR("ParticleEmitterComponent: no sprite assigned — emitter renders nothing");
            m_LoggedMissingSprite = true;
        }
        return false;
    }

    const int n = pool.AliveCount();
    if (n <= 0)
        return false;

    float anchorX, anchorY;
    AnchorFor(owner, anchorX, anchorY);

    Bounds b;
    if (!ComputeBounds(spr, anchorX, anchorY, b))
        return false;
    const int bboxW = b.maxX - b.minX;
    const int bboxH = b.maxY - b.minY;
    const float ppm = b.ppm;

    // RGB565A8 intermediate (3 bytes/pixel: [lo, hi, alpha]). It is the one
    // QuadBlit target that keeps coverage alpha (ARGB8888 targets write alpha
    // 0xFF on every touched pixel, so a soft particle edge came out opaque),
    // it is a quarter smaller than ARGB8888, and the final composite onto an
    // RGB565 framebuffer takes QuadBlit's RGB565A8 fast paths.
    const int bytesPerPixel = 3;
    const int needBytes = bboxW * bboxH * bytesPerPixel;
    if (needBytes > m_BboxBufBytes)
    {
        delete[] m_BboxBuf;
        m_BboxBuf = new uint8_t[needBytes];
        m_BboxBufBytes = needBytes;
    }
    // memset to 0 → alpha=0 (fully transparent) regardless of byte order.
    std::memset(m_BboxBuf, 0, needBytes);

    // Source descriptor for the sprite — same for every particle blit.
    const bool isRGB565 = (spr->format == Texture2D::TextureFormat::RGB565 ||
                           spr->format == Texture2D::TextureFormat::RGB565A8);
    const int srcBpp = Texture2D::GetBytesPerPixel(spr->format);
    QuadBlit::Source src = QuadBlit::MakeSource(
        spr->data, spr->width, spr->height,
        srcBpp, spr->hasAlpha, isRGB565,
        /*ownsPixels=*/false,
        spr->alphaRowSpans);
    if (spr->hasChromaKey)
    {
        src.hasChromaKey = true;
        src.keyR = spr->transparentR;
        src.keyG = spr->transparentG;
        src.keyB = spr->transparentB;
        if (isRGB565)
            DekiPixel::QuantizeRGB565(src.keyR, src.keyG, src.keyB);
        src.chromaRowSpans = spr->chromaRowSpans;
    }

    // Per-particle blit into the composite. Its clip stack is independent of
    // the framebuffer's; disable clip enforcement for this nested render so
    // scene clips don't suppress particles inside the composite (the final
    // blit is clipped as one sprite).
    const bool prevClipEnabled = QuadBlit::IsClipEnabled();
    QuadBlit::SetClipEnabled(false);

    const bool hasScale = pool.HasScale();
    const bool hasRotation = pool.HasRotation();
    const bool hasTint = pool.HasTint();
    const float* px = pool.posX;
    const float* py = pool.posY;
    const float* ps = pool.scale;
    const float* pr = pool.rotation;

    for (int i = 0; i < n; ++i)
    {
        // Metres -> composite pixels, relative to the box origin.
        const float lx = (px[i] - anchorX) * ppm - static_cast<float>(b.minX);
        const float ly = (py[i] - anchorY) * ppm - static_cast<float>(b.minY);
        const float s = hasScale ? ps[i] : 1.0f;
        const float r = hasRotation ? pr[i] : 0.0f;  // radians; Blit takes 0 through the scaled path
        uint8_t tR = 255, tG = 255, tB = 255, tA = 255;
        if (hasTint)
        {
            tR = pool.tintR[i];
            tG = pool.tintG[i];
            tB = pool.tintB[i];
            tA = pool.tintA[i];
            if (tA == 0) continue;
        }

        QuadBlit::Blit(
            src,
            m_BboxBuf, bboxW, bboxH, Deki::ColorFormat::RGB565A8,
            static_cast<int32_t>(lx), static_cast<int32_t>(ly),
            s, s, r,
            0.5f, 0.5f,
            tR, tG, tB, tA);
    }

    QuadBlit::SetClipEnabled(prevClipEnabled);

    // Hand the composite back to the framework. We keep m_BboxBuf for next
    // frame, so ownsPixels=false. Its pixels are at the sprite's scale.
    outSource = QuadBlit::MakeSource(
        m_BboxBuf, bboxW, bboxH,
        bytesPerPixel,
        /*hasAlpha=*/true,
        /*isRGB565=*/true,
        /*ownsPixels=*/false);
    outSource.pixelsPerMeter = ppm;

    // Pivot is the emitter's local origin within the box.
    outPivotX = -static_cast<float>(b.minX) / static_cast<float>(bboxW);
    outPivotY = -static_cast<float>(b.minY) / static_cast<float>(bboxH);
    return true;
}
