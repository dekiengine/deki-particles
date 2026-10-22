#include "ParticleEmitterComponent.h"
#include <deki/providers/Memory.h>
#include "PixelFormat.h"
#include "ParticleNodes.h"
#include "ParticleSystem.h"
#include <deki/Object.h>
#include <deki/Time.h>
#include <deki/LogSystem.h>
#include <deki/assets/Texture2D.h>
#include "deki-rendering/QuadBlit.h"
#include "deki-rendering/DekiRenderer.h"
#include "deki-rendering/DekiRenderPassRegistry.h"
#include "deki-rendering/RenderPass.h"
#include <deki/Engine.h>  // Deki::ColorFormat
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace DekiParticles
{

ParticleEmitterComponent::ParticleEmitterComponent()
{
    rng.Seed((uint32_t)(uintptr_t)this);
}

ParticleEmitterComponent::~ParticleEmitterComponent()
{
    ParticleSystem::GetInstance().UnregisterEmitter(this);
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
    if (m_ChainAttached)
    {
        return;
    }

    if (!RebuildChain())
    {
        return;  // no graph loaded yet; try again next tick
    }

    if (seed != 0)
    {
        rng.Seed(static_cast<uint32_t>(seed));
    }

    // Now that the capacity is known, let every modifier set up its private
    // state and request extra pool columns.
    for (const ParticleChainEntry& e : m_Chain)
    {
        if (e.ops->onAttach)
        {
            e.ops->onAttach(e.data, e.state, *this);
        }
    }
    m_ChainAttached = true;
}

void ParticleEmitterComponent::EnsurePoolAllocated()
{
    if (m_PoolAllocated && pool.Capacity() == maxParticles)
    {
        return;
    }
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
    {
        return false;  // not loaded yet, which is fine: assets resolve later
    }

    const char* error = nullptr;
    if (BuildParticleChain(g->data->Root(), Deki::HashString(ParticleEmitNode::StaticNodeName), m_Chain, &error))
    {
        return true;
    }

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
    {
        if (e.ops->onAttach)
        {
            e.ops->onAttach(e.data, e.state, *this);
        }
    }
    m_ChainAttached = true;
}
#endif

int ParticleEmitterComponent::Spawn()
{
    int idx = pool.Spawn();
    if (idx < 0)
    {
        return -1;
    }

    // Reset this slot's state. All numeric state is float.
    pool.posX[idx] = 0.0f;
    pool.posY[idx] = 0.0f;
    pool.velX[idx] = 0.0f;
    pool.velY[idx] = 0.0f;
    pool.age[idx] = 0.0f;
    pool.lifetime[idx] = 1.0f;
    if (pool.HasRotation())
    {
        pool.rotation[idx] = 0.0f;
        pool.rotationSpeed[idx] = 0.0f;
    }
    if (pool.HasScale())
    {
        pool.scale[idx] = 1.0f;
    }
    if (pool.HasTint())
    {
        pool.tintR[idx] = 255;
        pool.tintG[idx] = 255;
        pool.tintB[idx] = 255;
        pool.tintA[idx] = 255;
    }

    // Run onEmit on every enabled modifier in chain order. The Emission node
    // is the usual caller; the spawn-time setters wired after it read the
    // lifetime it set and write the initial position, velocity and rotation.
    for (const ParticleChainEntry& e : m_Chain)
    {
        if (!e.ops->onEmit)
        {
            continue;
        }
        if (e.ops->isEnabled && !e.ops->isEnabled(e.data))
        {
            continue;
        }
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
    // Set up here too, so the editor preview works without Start(); the engine
    // drives the lifecycle only in Play mode.
    EnsureReady();

    if (m_Chain.empty())
    {
        return;
    }

    if (dt <= 0.0f)
    {
        return;
    }
    if (dt > 0.1f)
    {
        dt = 0.1f;  // clamp huge deltas (loading, paused...) so particles do not jump
    }

    // Age and kill expired particles before the simulation hooks, so
    // OnSimulate visits only live particles. age and lifetime are float;
    // convert dt once before the loop.
    float dtN = static_cast<float>(dt);
    int n = pool.AliveCount();
    for (int i = 0; i < n;)
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
        if (!e.ops->onSimulate)
        {
            continue;
        }
        if (e.ops->isEnabled && !e.ops->isEnabled(e.data))
        {
            continue;
        }
        e.ops->onSimulate(e.data, e.state, *this, dt);
    }

    // Integrate (pos += vel * dt) once, after every force modifier has changed
    // velocity. Always runs, even with no modifiers, so a particle from a
    // programmatic Spawn() with a velocity still moves.
    int alive = pool.AliveCount();
    float* px = pool.posX.Data();
    float* py = pool.posY.Data();
    float* vx = pool.velX.Data();
    float* vy = pool.velY.Data();
    // dtN is the dt converted for the ageing loop above.
    for (int i = 0; i < alive; ++i)
    {
        px[i] = ((px[i]) + (((vx[i]) * (dtN))));
        py[i] = ((py[i]) + (((vy[i]) * (dtN))));
    }
}

#ifdef DEKI_EDITOR
void ParticleEmitterComponent::EditorPreviewRestart()
{
    // Kill the live particles but keep the pool, so its capacity survives
    // restarts.
    if (m_PoolAllocated)
    {
        // Drain to zero without touching the column pointers.
        while (pool.AliveCount() > 0)
        {
            pool.KillSwap(pool.AliveCount() - 1);
        }
    }
    // Rebuild the chain from the loaded graph, so every modifier's state blob
    // starts over (the burst latch, the rate accumulator) and a reimported
    // graph takes effect. Clearing the attach flag makes EnsureReady run the
    // whole pass again.
    m_ChainAttached = false;
    m_LoggedBadGraph = false;
    EnsureReady();
}
#endif

void ParticleEmitterComponent::UnloadAssets()
{
    sprite.ptr = nullptr;
    sprite.loadAttempted = false;

    // The chain points into the graph asset's node instances, so it must not
    // outlive the asset. Drop it before releasing the reference.
    FreeChain();
    m_ChainAttached = false;
    graph.ptr = nullptr;
    graph.loadAttempted = false;

    m_LoggedMissingSprite = false;
    m_LoggedBadGraph = false;
}

namespace
{
// Composite pixels per world metre: the sprite's own scale, so the emitter's
// output is 1:1 with the art whenever the camera runs at the sprite's ppm.
inline float SpritePpm(const Deki2D::Sprite* spr)
{
    return (spr && spr->pixelsPerMeter > 0.0f) ? spr->pixelsPerMeter : 1.0f;
}

// A particle's composite row offset from the emitter, in pixels. World Y
// points up and composite rows go down, so a higher particle gets a lower row.
inline float CompositeY(float worldY, float anchorY, float ppm)
{
    return (anchorY - worldY) * ppm;
}
}  // namespace

void ParticleEmitterComponent::AnchorFor(const Deki::Object* owner, float& anchorX, float& anchorY) const
{
    // worldSpace true: particles store world coordinates; subtract the
    // emitter's world origin so the composite sits at the emitter after the
    // final transform. worldSpace false: particles are already emitter-local.
    anchorX = 0.0f;
    anchorY = 0.0f;
    if (worldSpace && owner)
    {
        anchorX = owner->GetWorldX();
        anchorY = owner->GetWorldY();
    }
}

bool ParticleEmitterComponent::ComputeBounds(const Deki2D::Sprite* spr, float anchorX, float anchorY, Bounds& out) const
{
    const int n = pool.AliveCount();
    if (n <= 0 || !spr)
    {
        return false;
    }

    const float ppm = SpritePpm(spr);
    const float spriteW = static_cast<float>(spr->width);
    const float spriteH = static_cast<float>(spr->height);
    // Rotation grows the box by up to sqrt(2); 1.45 leaves a one-pixel margin.
    constexpr float kRotPad = 1.45f;
    const bool hasScale = pool.HasScale();
    const float* px = pool.posX.Data();
    const float* py = pool.posY.Data();
    const float* ps = pool.scale.Data();

    float minX = 1e9f, minY = 1e9f;
    float maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i)
    {
        const float lx = (px[i] - anchorX) * ppm;  // metres -> composite pixels
        const float ly = CompositeY(py[i], anchorY, ppm);
        const float s = hasScale ? ps[i] : 1.0f;
        const float halfW = 0.5f * spriteW * s * kRotPad;
        const float halfH = 0.5f * spriteH * s * kRotPad;
        if (lx - halfW < minX)
        {
            minX = lx - halfW;
        }
        if (ly - halfH < minY)
        {
            minY = ly - halfH;
        }
        if (lx + halfW > maxX)
        {
            maxX = lx + halfW;
        }
        if (ly + halfH > maxY)
        {
            maxY = ly + halfH;
        }
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
    const Deki2D::Sprite* spr = sprite.Get();
    if (!spr || !spr->data)
    {
        return false;  // DrawParticles logs the missing sprite
    }

    if (pool.AliveCount() <= 0)
    {
        outWidth = outHeight = 0.0f;  // nothing to draw: skip the RenderContent call too
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
    // origin in every direction. The box is not centred on the emitter, so
    // report the farther edge on each axis.
    const float reachX = static_cast<float>(std::max(std::abs(b.minX), std::abs(b.maxX)));
    const float reachY = static_cast<float>(std::max(std::abs(b.minY), std::abs(b.maxY)));
    outWidth = reachX / b.ppm;
    outHeight = reachY / b.ppm;
    return true;
}

bool ParticleEmitterComponent::RenderContent(const Deki::Object*, QuadBlit::Source&, float&, float&, uint8_t&,
                                             uint8_t&, uint8_t&, uint8_t&)
{
    return false;
}

void ParticleEmitterComponent::DrawParticles(const Deki::Object* owner, DekiRendering::RenderContext& ctx)
{
    Deki2D::Sprite* spr = sprite.Get();
    if (!spr || !spr->data)
    {
        if (!m_LoggedMissingSprite)
        {
            DEKI_LOG_ERROR("ParticleEmitterComponent: no sprite assigned — emitter renders nothing");
            m_LoggedMissingSprite = true;
        }
        return;
    }
    const int n = pool.AliveCount();
    if (n <= 0 || !owner || !ctx.buffer || !ctx.cam.valid)
    {
        return;
    }

    // The sprite's source, the same for every particle.
    const bool isRGB565 = (spr->format == Deki::Texture2D::TextureFormat::RGB565 ||
                           spr->format == Deki::Texture2D::TextureFormat::RGB565A8);
    QuadBlit::Source src = QuadBlit::MakeSource(spr->data, spr->width, spr->height,
                                                QuadBlit::PixelLayout::FromTexture(spr->format, spr->hasAlpha),
                                                /*ownsPixels=*/false, spr->alphaRowSpans.Data());
    if (spr->hasChromaKey)
    {
        src.hasChromaKey = true;
        src.keyR = spr->transparentR;
        src.keyG = spr->transparentG;
        src.keyB = spr->transparentB;
        if (isRGB565)
        {
            DekiPixel::QuantizeRGB565(src.keyR, src.keyG, src.keyB);
        }
        src.chromaRowSpans = spr->chromaRowSpans.Data();
    }

    // Worked out in the whole view, as the renderer's own draws are, and moved
    // to the strip (ctx.viewTop) in whole pixels at the end.
    DekiRendering::FrameCamera cam = ctx.cam;
    cam.halfH += static_cast<float>(ctx.viewTop);
    const int32_t viewTop = ctx.viewTop;

    // A particle sits where the emitter's transform puts its offset from the
    // anchor (the emitter's origin for world-space particles), scaled, then
    // rotated, as a child object would be. Its sprite takes the emitter's
    // scale and rotation on top of its own.
    const Deki::WorldTransform wt = owner->GetWorldTransform();
    float anchorX, anchorY;
    AnchorFor(owner, anchorX, anchorY);
    float originX, originY;
    cam.WorldToScreen(wt.x, wt.y, originX, originY);
    const float cosR = std::cos(wt.rotation);
    const float sinR = std::sin(wt.rotation);
    const float ppm = cam.ppm;
    const float toSprite = ppm / SpritePpm(spr);  // screen pixels per sprite pixel at scale 1
    const bool snap = pixelSnap || cam.snapStep > 0;
    const bool dither = (alphaMode == DekiRendering::AlphaMode::OrderedDither);

    // A particle wholly outside the target (a strip, often) is skipped before
    // any blit work: its reach is the sprite's half diagonal at its scale.
    const float halfDiagonal = 0.5f * std::sqrt(static_cast<float>(spr->width) * spr->width +
                                                static_cast<float>(spr->height) * spr->height);
    const float emitterScale = std::max(std::abs(wt.scaleX), std::abs(wt.scaleY));

    const bool wasClipEnabled = QuadBlit::IsClipEnabled();
    if (ignoreClip)
    {
        QuadBlit::SetClipEnabled(false);
    }

    const bool hasScale = pool.HasScale();
    const bool hasRotation = pool.HasRotation();
    const bool hasTint = pool.HasTint();
    const float* px = pool.posX.Data();
    const float* py = pool.posY.Data();
    const float* ps = pool.scale.Data();
    const float* pr = pool.rotation.Data();

    for (int i = 0; i < n; ++i)
    {
        uint8_t tR = 255, tG = 255, tB = 255, tA = 255;
        if (hasTint)
        {
            tR = pool.tintR[i];
            tG = pool.tintG[i];
            tB = pool.tintB[i];
            tA = pool.tintA[i];
            if (tA == 0)
            {
                continue;
            }
        }
        const float s = hasScale ? ps[i] : 1.0f;
        const float r = hasRotation ? pr[i] : 0.0f;

        const float ox = (px[i] - anchorX) * wt.scaleX;
        const float oy = (py[i] - anchorY) * wt.scaleY;
        float screenX = originX + (ox * cosR - oy * sinR) * ppm;
        float screenY = originY - (ox * sinR + oy * cosR) * ppm;

        const float reach = halfDiagonal * std::abs(s) * emitterScale * toSprite + 2.0f;
        if (screenX + reach < 0.0f || screenX - reach > static_cast<float>(ctx.width) ||
            screenY - static_cast<float>(viewTop) + reach < 0.0f ||
            screenY - static_cast<float>(viewTop) - reach > static_cast<float>(ctx.height))
        {
            continue;
        }

        if (cam.snapStep > 0)
        {
            screenX = cam.SnapX(screenX);
            screenY = cam.SnapY(screenY);
        }
        const int32_t ix = snap ? static_cast<int32_t>(std::lround(screenX)) : static_cast<int32_t>(screenX);
        const int32_t iy =
            (snap ? static_cast<int32_t>(std::lround(screenY)) : static_cast<int32_t>(screenY)) - viewTop;

        QuadBlit::Blit(src, ctx.buffer, ctx.width, ctx.height, ctx.format, ix, iy, s * wt.scaleX * toSprite,
                       s * wt.scaleY * toSprite, r + wt.rotation, 0.5f, 0.5f, tR, tG, tB, tA, dither);
    }

    if (ignoreClip)
    {
        QuadBlit::SetClipEnabled(wasClipEnabled);
    }
}

// Draws each emitter's particles when the renderer reaches it in sort order.
// Attached whenever this package is loaded (autoAttach), like the tilemap
// pass, so no project has to name it in its render pipeline.
class ParticleRenderPass : public DekiRendering::RenderPass
{
public:
    static constexpr const char* kRegistryName = "particles";

    uint32_t HookMask() const override { return DekiRendering::RenderPassHooks::Execute; }

    void Execute(Deki::Object* obj, DekiRendering::RenderContext& ctx) override
    {
        if (auto* emitter = obj->GetComponent<ParticleEmitterComponent>())
        {
            emitter->DrawParticles(obj, ctx);
        }
    }
};

static struct ParticleRenderPassRegistrar
{
    ParticleRenderPassRegistrar()
    {
        DekiRendering::RenderPassInfo info;
        info.factory = []() -> DekiRendering::RenderPass* { return new ParticleRenderPass(); };
        info.autoAttach = true;
        DekiRendering::DekiRenderPassRegistry::Register(ParticleRenderPass::kRegistryName, info);
    }
    // Required so the registry does not outlive this DLL's code.
    ~ParticleRenderPassRegistrar() { DekiRendering::DekiRenderPassRegistry::Unregister(ParticleRenderPass::kRegistryName); }
} s_ParticleRenderPassRegistrar;

}  // namespace DekiParticles
