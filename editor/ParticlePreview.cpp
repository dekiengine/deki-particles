// Live preview of a particle graph inside the Node Graph window.
//
// It runs the effect being edited, not the saved asset: the window passes the
// live document each tick, so a value typed in the properties panel shows on
// the next frame. That works without rebuilding because the chain points at
// the document's node instances.
//
// The chain is rebuilt only when the graph's topology changes (a node added,
// deleted or rewired), tracked by a cheap signature. Rebuilding every frame
// would reallocate the state blobs and reset every accumulator, and an
// emission rate reset every frame never emits.
//
// Particles are drawn as plain dots, not sprites: the sprite belongs to
// ParticleEmitterComponent, not the graph, so a graph alone has no texture.
// Motion, spread, gravity, drag, size and colour all show on dots; only the
// artwork is missing.
//
// The emitter's shape is outlined under them at the same scale, so a radius
// is seen against the spray it produces. It is drawn whether or not the chain
// builds, since that is when wiring it up.

#ifdef DEKI_EDITOR

#include "ParticleEmitterComponent.h"
#include "ParticleChain.h"
#include "ParticleNodes.h"

#include "deki-nodegraph/DekiNode.h"
#include "deki-nodegraph/NodeGraphPreview.h"

#include <cmath>
#include <cstdint>
#include <vector>

// Editor extensions live in DekiEditor; the package's own types are in DekiParticles.
using namespace DekiParticles;

namespace
{

// The preview's own pool size. maxParticles is a component property the graph
// does not have, so the preview picks a ceiling high enough that a burst is
// not cut short while you tune it.
constexpr int kPreviewMaxParticles = 512;

struct ParticlePreviewState
{
    ParticleEmitterComponent emitter;
    uint64_t topology = 0;  // signature of the last graph the chain was built from
    bool built = false;
};

// Cheap signature of what the chain depends on: which nodes exist, of what
// type, and how they are wired. Property values are left out on purpose: the
// chain points at the live instances, so an edited value needs no rebuild and
// must not cause one.
uint64_t TopologyOf(const DekiNodeGraph::NodeGraphPreviewGraph& graph)
{
    uint64_t h = 1469598103934665603ull;  // FNV-1a 64
    auto mix = [&h](uint64_t v)
    {
        h ^= v;
        h *= 1099511628211ull;
    };
    for (int i = 0; i < graph.nodeCount; ++i)
    {
        mix(graph.nodes[i].id);
        mix(graph.nodes[i].typeId);
        mix(reinterpret_cast<uint64_t>(graph.nodes[i].instance));
    }
    for (int i = 0; i < graph.linkCount; ++i)
    {
        mix(graph.links[i].fromNode);
        mix(static_cast<uint64_t>(graph.links[i].fromPin));
        mix(graph.links[i].toNode);
        mix(static_cast<uint64_t>(graph.links[i].toPin));
    }
    return h;
}

// A circle outline as a polyline: the canvas offers only a filled circle and
// lines, and a shape gizmo needs an outline.
void StrokeCircle(const DekiNodeGraph::NodeGraphPreviewCanvas& canvas, float cx, float cy, float r, uint32_t rgba,
                  float thickness)
{
    if (r <= 0.5f)
    {
        return;
    }
    int seg = static_cast<int>(r * 0.9f);
    if (seg < 16)
    {
        seg = 16;
    }
    if (seg > 96)
    {
        seg = 96;
    }
    float px = cx + r, py = cy;
    for (int i = 1; i <= seg; ++i)
    {
        const float a = (Deki::Math::kTwoPi * static_cast<float>(i)) / static_cast<float>(seg);
        const float qx = cx + std::cos(a) * r;
        const float qy = cy - std::sin(a) * r;
        canvas.line(canvas.ctx, px, py, qx, qy, rgba, thickness);
        px = qx;
        py = qy;
    }
}

// The emitter's shape, drawn where particles are born and at the scale they
// move in. Here a radius is a real size, not a proportion: the panel's px/m
// slider scales the outline and the particles together, so a 0.3 m circle
// looks 0.3 m next to its spray. Faint and drawn under the particles, so the
// effect stays the focus.
void DrawEmitterShape(const DekiNodeGraph::NodeGraphPreviewGraph& graph, float cx, float cy, float pixelsPerMeter,
                      const DekiNodeGraph::NodeGraphPreviewCanvas& canvas)
{
    const DekiNodeGraph::NodeGraphPreviewNode* node =
        graph.FindFirstOfType(Deki::HashString(ParticleEmissionNode::StaticNodeName));
    if (!node || !node->instance)
    {
        return;
    }
    const auto& e = *static_cast<const ParticleEmissionNode*>(node->instance);

    const uint32_t line = DekiNodeGraph::NodeGraphPreviewRgba(122, 190, 255, e.enabled ? 90 : 40);
    const uint32_t mark = DekiNodeGraph::NodeGraphPreviewRgba(255, 255, 255, 60);

    switch (e.shape)
    {
        case EmitterShapeKind::Circle: StrokeCircle(canvas, cx, cy, e.radius * pixelsPerMeter, line, 1.0f); break;
        case EmitterShapeKind::Rect:
        {
            const float hw = e.width * 0.5f * pixelsPerMeter;
            const float hh = e.height * 0.5f * pixelsPerMeter;
            if (hw > 0.5f || hh > 0.5f)
            {
                canvas.line(canvas.ctx, cx - hw, cy - hh, cx + hw, cy - hh, line, 1.0f);
                canvas.line(canvas.ctx, cx + hw, cy - hh, cx + hw, cy + hh, line, 1.0f);
                canvas.line(canvas.ctx, cx + hw, cy + hh, cx - hw, cy + hh, line, 1.0f);
                canvas.line(canvas.ctx, cx - hw, cy + hh, cx - hw, cy - hh, line, 1.0f);
            }
            break;
        }
        case EmitterShapeKind::Point:
        default: break;
    }

    // Origin, always: with a Point shape it is the whole gizmo, and with the
    // others it says which way the effect is offset from its centre.
    canvas.line(canvas.ctx, cx - 4.0f, cy, cx + 4.0f, cy, mark, 1.0f);
    canvas.line(canvas.ctx, cx, cy - 4.0f, cx, cy + 4.0f, mark, 1.0f);
}

void* PreviewCreate()
{
    auto* p = new ParticlePreviewState();
    p->emitter.maxParticles = kPreviewMaxParticles;
    // No owner object here, so world space has no origin to add: local space
    // puts the effect at the preview's centre. EmissionEmit also checks
    // GetOwner(), so this is a second safeguard.
    p->emitter.worldSpace = false;
    return p;
}

void PreviewDestroy(void* preview)
{
    delete static_cast<ParticlePreviewState*>(preview);
}

void PreviewReset(void* preview)
{
    auto* p = static_cast<ParticlePreviewState*>(preview);
    // Drop every live particle and force a rebuild, which zeroes the state
    // blobs again (burst latch, rate accumulator).
    while (p->emitter.pool.AliveCount() > 0)
    {
        p->emitter.pool.KillSwap(p->emitter.pool.AliveCount() - 1);
    }
    p->built = false;
    p->topology = 0;
}

void PreviewTick(void* preview, const DekiNodeGraph::NodeGraphPreviewGraph& graph, float dt, float x, float y, float w,
                 float h, float pixelsPerMeter, const DekiNodeGraph::NodeGraphPreviewCanvas& canvas)
{
    auto* p = static_cast<ParticlePreviewState*>(preview);

    const uint64_t topology = TopologyOf(graph);
    if (!p->built || topology != p->topology)
    {
        std::vector<DekiParticles::ParticleChainEntry> chain;
        const char* error = nullptr;
        // A half-wired graph is normal while authoring, so a failed build is
        // not logged; the panel just shows nothing.
        if (DekiParticles::BuildParticleChain(graph, Deki::HashString(ParticleEmitNode::StaticNodeName), chain, &error))
        {
            p->emitter.AdoptChain(std::move(chain));
            p->built = true;
        }
        else
        {
            p->built = false;
        }
        p->topology = topology;
    }

    if (p->built && dt > 0.0f)
    {
        p->emitter.Simulate(dt);
    }

    // Origin at the centre of the preview rect; world Y up maps to screen Y
    // down.
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;

    // Before the early-out: a half-wired graph draws no particles, and the
    // shape is what you want to see while wiring it up.
    DrawEmitterShape(graph, cx, cy, pixelsPerMeter, canvas);

    if (!p->built)
    {
        return;
    }

    auto& pool = p->emitter.pool;
    const int n = pool.AliveCount();
    const float baseRadius = 2.5f;

    for (int i = 0; i < n; ++i)
    {
        const float px = cx + pool.posX[i] * pixelsPerMeter;
        const float py = cy - pool.posY[i] * pixelsPerMeter;

        const float scale = pool.HasScale() ? pool.scale[i] : 1.0f;
        float radius = baseRadius * scale;
        if (radius < 0.75f)
        {
            radius = 0.75f;
        }

        // Skip what falls outside the panel. The window clips anyway; this
        // saves draw calls for an effect that flies off screen.
        if (px + radius < x || px - radius > x + w || py + radius < y || py - radius > y + h)
        {
            continue;
        }

        uint8_t r = 255, g = 255, b = 255, a = 255;
        if (pool.HasTint())
        {
            r = pool.tintR[i];
            g = pool.tintG[i];
            b = pool.tintB[i];
            a = pool.tintA[i];
        }
        if (a == 0)
        {
            continue;
        }

        const uint32_t rgba = DekiNodeGraph::NodeGraphPreviewRgba(r, g, b, a);

        canvas.circleFilled(canvas.ctx, px, py, radius, rgba);

        // A dot cannot show spin, so rotating particles get a spoke; otherwise
        // Initial Rotation and Rotation over Lifetime would seem to do nothing.
        if (pool.HasRotation() && radius >= 2.0f)
        {
            const float ang = pool.rotation[i];
            canvas.line(canvas.ctx, px, py, px + std::cos(ang) * radius, py - std::sin(ang) * radius, rgba, 1.0f);
        }
    }
}

DekiNodeGraph::NodeGraphPreviewOps MakePreviewOps()
{
    DekiNodeGraph::NodeGraphPreviewOps ops;
    ops.create = &PreviewCreate;
    ops.destroy = &PreviewDestroy;
    ops.reset = &PreviewReset;
    ops.tick = &PreviewTick;
    return ops;
}

}  // namespace

DekiNodeGraph::NodeGraphPreviewOps DekiParticlesPreviewOps()
{
    static const DekiNodeGraph::NodeGraphPreviewOps kOps = MakePreviewOps();
    return kOps;
}

#endif  // DEKI_EDITOR
