#include "ParticleEmitterComponent.h"
#include "ParticleModifier.h"
#include "ParticleSystem.h"
#include "DekiObject.h"
#include "DekiTime.h"
#include "DekiLogSystem.h"
#include "deki-2d/Texture2D.h"
#include "deki-rendering/QuadBlit.h"
#include "DekiEngine.h"  // for DekiColorFormat enum
#include <algorithm>
#include <cmath>
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
    if (m_ModifiersAttached) return;

    RefreshModifiers();
    if (m_OnSimulate.empty()) return;  // No siblings yet — try again next tick.

    // Give every modifier a chance to allocate its private state and request
    // pool extension columns now that capacity is known. Same set is shared
    // between m_OnSimulate and m_OnEmit, so iterate one to avoid double-attach.
    for (auto* m : m_OnSimulate)
        m->OnAttachToEmitter(*this);
    m_ModifiersAttached = true;
}

void ParticleEmitterComponent::EnsurePoolAllocated()
{
    if (m_PoolAllocated && pool.Capacity() == maxParticles)
        return;
    pool.SetCapacity(maxParticles);
    m_PoolAllocated = true;
}

void ParticleEmitterComponent::RefreshModifiers()
{
    m_OnEmit.clear();
    m_OnSimulate.clear();

    DekiObject* owner = GetOwner();
    if (!owner) return;

    // Collect every sibling ParticleModifier. We can't query by base type
    // directly with GetComponent<>, so iterate all components and dynamic_cast.
    // Modifier counts are tiny (< ~20) so this is cheap.
    auto& comps = owner->GetComponents();
    std::vector<ParticleModifier*> all;
    all.reserve(comps.size());
    for (auto* c : comps)
    {
        if (auto* m = dynamic_cast<ParticleModifier*>(c))
            all.push_back(m);
    }

    std::stable_sort(all.begin(), all.end(),
        [](ParticleModifier* a, ParticleModifier* b) {
            int pa = a->GetSimulationPhase();
            int pb = b->GetSimulationPhase();
            if (pa != pb) return pa < pb;
            return a->order < b->order;
        });

    // Same list reused for emit and simulate hook order (phase ordering
    // applies identically). If a modifier wants to opt out of one hook it
    // simply leaves the default empty body.
    m_OnEmit = all;
    m_OnSimulate = all;
}

int ParticleEmitterComponent::Spawn()
{
    int idx = pool.Spawn();
    if (idx < 0) return -1;

    // Reset particle state for this slot.
    pool.posX[idx] = 0.0f;
    pool.posY[idx] = 0.0f;
    pool.velX[idx] = 0.0f;
    pool.velY[idx] = 0.0f;
    pool.age[idx] = 0.0f;
    pool.lifetime[idx] = 1.0f;
    if (pool.HasRotation())      { pool.rotation[idx] = 0.0f; pool.rotationSpeed[idx] = 0.0f; }
    if (pool.HasScale())         { pool.scale[idx] = 1.0f; }
    if (pool.HasTint())          { pool.tintR[idx] = 255; pool.tintG[idx] = 255; pool.tintB[idx] = 255; pool.tintA[idx] = 255; }

    // Drive OnEmit through every ENABLED modifier in phase order.
    // EmissionModifier (phase 0) is the typical caller; spawn-time setters
    // (phase 10+) read EmissionModifier-set lifetime and write initial
    // pos/vel/rot/etc.
    for (auto* m : m_OnEmit)
        if (m->enabled) m->OnEmit(*this, idx);

    return idx;
}

void ParticleEmitterComponent::Update()
{
    float dt = DekiTime::GetDeltaTimeF() / 1000.0f;
    Simulate(dt);
}

void ParticleEmitterComponent::Simulate(float dt)
{
    // Self-bootstrap so the editor preview path works without Start() ever
    // having fired (Play mode is the only context where the engine drives
    // the lifecycle).
    EnsureReady();

    if (m_OnSimulate.empty())
        return;

    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;  // Clamp huge deltas (loading, paused, etc.) to avoid teleport.

    // Age + kill expired particles BEFORE driving simulation hooks. This way
    // OnSimulate iterates only currently-alive particles.
    int n = pool.AliveCount();
    for (int i = 0; i < n; )
    {
        pool.age[i] += dt;
        if (pool.age[i] >= pool.lifetime[i])
        {
            pool.KillSwap(i);
            n--;
            continue;
        }
        ++i;
    }

    for (auto* m : m_OnSimulate)
        if (m->enabled) m->OnSimulate(*this, dt);

    // Final kinematic integration (pos += vel * dt) — runs once after all
    // force modifiers have mutated velocity. Always happens, even with no
    // modifiers attached, so that a programmatic Spawn() with non-zero
    // velocity still moves.
    int alive = pool.AliveCount();
    float* px = pool.posX; float* py = pool.posY;
    float* vx = pool.velX; float* vy = pool.velY;
    for (int i = 0; i < alive; ++i)
    {
        px[i] += vx[i] * dt;
        py[i] += vy[i] * dt;
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
    // Re-run OnAttachToEmitter on every modifier so per-modifier sim state
    // (EmissionModifier's burst latch, rate accumulator, etc.) resets cleanly.
    // Clear the attach flag so EnsureReady re-runs the attach pass.
    m_ModifiersAttached = false;
    EnsureReady();
}
#endif

void ParticleEmitterComponent::UnloadAssets()
{
    sprite.ptr = nullptr;
    sprite.loadAttempted = false;
    FreeBboxBuf();
    m_LoggedMissingSprite = false;
}

void ParticleEmitterComponent::FreeBboxBuf()
{
    delete[] m_BboxBuf;
    m_BboxBuf = nullptr;
    m_BboxBufBytes = 0;
}

bool ParticleEmitterComponent::RenderContent(const DekiObject* owner,
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

    int n = pool.AliveCount();
    if (n <= 0)
        return false;

    // Anchor for converting particle positions to bbox-local coordinates.
    // worldSpace=true: particles store world coords; subtract emitter's world
    // origin so the bbox sits at the emitter's location after final transform.
    // worldSpace=false: particles store emitter-local coords; no subtraction.
    float anchorX = 0.0f, anchorY = 0.0f;
    if (worldSpace && owner)
    {
        anchorX = owner->GetWorldX();
        anchorY = owner->GetWorldY();
    }

    const float spriteW = (float)spr->width;
    const float spriteH = (float)spr->height;
    // Rotation expands AABB up to sqrt(2). 1.45 leaves a one-pixel guard band.
    constexpr float kRotPad = 1.45f;

    // First pass: tight integer bbox over all alive particles.
    float minX =  1e9f, minY =  1e9f;
    float maxX = -1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i)
    {
        float lx = pool.posX[i] - anchorX;
        float ly = pool.posY[i] - anchorY;
        float s  = pool.HasScale() ? pool.scale[i] : 1.0f;
        float halfW = 0.5f * spriteW * s * kRotPad;
        float halfH = 0.5f * spriteH * s * kRotPad;
        if (lx - halfW < minX) minX = lx - halfW;
        if (ly - halfH < minY) minY = ly - halfH;
        if (lx + halfW > maxX) maxX = lx + halfW;
        if (ly + halfH > maxY) maxY = ly + halfH;
    }

    int bboxMinX = (int)std::floor(minX);
    int bboxMinY = (int)std::floor(minY);
    int bboxMaxX = (int)std::ceil(maxX);
    int bboxMaxY = (int)std::ceil(maxY);
    int bboxW = bboxMaxX - bboxMinX;
    int bboxH = bboxMaxY - bboxMinY;
    if (bboxW <= 0 || bboxH <= 0)
        return false;

    // ARGB8888 intermediate (4 bytes/pixel). QuadBlit's only RGB565-family
    // target is plain RGB565 with no alpha, which can't accumulate alpha-blended
    // particles. ARGB8888 is the smallest target format that preserves alpha.
    const int bytesPerPixel = 4;
    int needBytes = bboxW * bboxH * bytesPerPixel;
    if (needBytes > m_BboxBufBytes)
    {
        delete[] m_BboxBuf;
        m_BboxBuf = new uint8_t[needBytes];
        m_BboxBufBytes = needBytes;
    }
    // memset to 0 → alpha=0 (fully transparent) regardless of byte order.
    std::memset(m_BboxBuf, 0, needBytes);

    // Source descriptor for the sprite — same for every particle blit.
    bool isRGB565 = (spr->format == Texture2D::TextureFormat::RGB565 ||
                     spr->format == Texture2D::TextureFormat::RGB565A8);
    int srcBpp = Texture2D::GetBytesPerPixel(spr->format);
    QuadBlit::Source src = QuadBlit::MakeSource(
        spr->data, spr->width, spr->height,
        srcBpp, spr->has_alpha, isRGB565,
        /*ownsPixels=*/false,
        spr->alphaRowSpans);
    if (spr->has_chroma_key)
    {
        src.hasChromaKey = true;
        if (isRGB565)
        {
            src.keyR = (uint8_t)((spr->transparent_r >> 3) << 3);
            src.keyG = (uint8_t)((spr->transparent_g >> 2) << 2);
            src.keyB = (uint8_t)((spr->transparent_b >> 3) << 3);
        }
        else
        {
            src.keyR = spr->transparent_r;
            src.keyG = spr->transparent_g;
            src.keyB = spr->transparent_b;
        }
        src.chromaRowSpans = spr->chromaRowSpans;
    }

    // Per-particle blit into the bbox buffer. The bbox buffer's clip stack
    // is independent of the framebuffer's; we disable clip enforcement
    // for this nested render so global scene clips don't accidentally
    // suppress particles outside the prefab view.
    bool prevClipEnabled = QuadBlit::IsClipEnabled();
    QuadBlit::SetClipEnabled(false);

    for (int i = 0; i < n; ++i)
    {
        float lx = pool.posX[i] - anchorX - (float)bboxMinX;
        float ly = pool.posY[i] - anchorY - (float)bboxMinY;
        float s  = pool.HasScale() ? pool.scale[i] : 1.0f;
        float r  = pool.HasRotation() ? pool.rotation[i] : 0.0f;
        uint8_t tR = pool.HasTint() ? pool.tintR[i] : 255;
        uint8_t tG = pool.HasTint() ? pool.tintG[i] : 255;
        uint8_t tB = pool.HasTint() ? pool.tintB[i] : 255;
        uint8_t tA = pool.HasTint() ? pool.tintA[i] : 255;
        if (tA == 0) continue;

        QuadBlit::Blit(
            src,
            m_BboxBuf, bboxW, bboxH, DekiColorFormat::ARGB8888,
            (int32_t)lx, (int32_t)ly,
            s, s, r,
            0.5f, 0.5f,
            tR, tG, tB, tA);
    }

    QuadBlit::SetClipEnabled(prevClipEnabled);

    // Hand the bbox buffer back to the framework. The framework owns nothing —
    // we keep m_BboxBuf around for next frame, so ownsPixels=false.
    outSource = QuadBlit::MakeSource(
        m_BboxBuf, bboxW, bboxH,
        bytesPerPixel,
        /*hasAlpha=*/true,
        /*isRGB565=*/false,
        /*ownsPixels=*/false);

    // Pivot is the emitter's local origin within the bbox.
    outPivotX = (bboxW > 0) ? (-(float)bboxMinX / (float)bboxW) : 0.5f;
    outPivotY = (bboxH > 0) ? (-(float)bboxMinY / (float)bboxH) : 0.5f;
    return true;
}
