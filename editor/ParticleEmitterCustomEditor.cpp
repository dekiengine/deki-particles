/**
 * @file ParticleEmitterCustomEditor.cpp
 * @brief Inspector override for ParticleEmitterComponent.
 *
 *  - Auto-adds an EmissionModifier sibling if missing (a particle system
 *    without emission spawns nothing — that's never the user's intent).
 *  - Renders a preview transport (play / pause / step / restart) plus a
 *    speed slider that drives editor-mode simulation via OnEditorUpdate.
 *  - Lists modifier siblings as a Unity-style modules section with phase
 *    badges, friendly names, enable checkboxes, reorder arrows, and an
 *    "Add Module" popup grouped by phase.
 *  - EmissionModifier's Remove button is disabled — the system depends on it.
 *
 * Modifiers stay sibling DekiBehaviour components on the same DekiObject,
 * so each still has its own card below the emitter for property editing.
 */

#ifdef DEKI_EDITOR

#include <deki-editor/EditorRegistry.h>
#include <deki-editor/CustomEditor.h>
#include <deki-editor/EditorUI.h>
#include <deki-editor/PrefabView.h>
#include "ParticleEmitterComponent.h"
#include "ParticleModifier.h"
#include "EmissionModifier.h"
#include "DekiObject.h"
#include "Prefab.h"
#include "DekiTime.h"
#include "reflection/ComponentRegistry.h"
// ImGui is provided transitively by <deki-editor/CustomEditor.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <chrono>

// Lucide icons (UTF-8 bytes from deki-editor's IconsLucide.h — inlined here so
// the module doesn't depend on an editor-private header). Glyphs are merged
// into the default ImGui font, so we just embed the bytes in button labels.
#define LUCIDE_PLAY        "\xee\x84\xbc"
#define LUCIDE_PAUSE       "\xee\x84\xae"
#define LUCIDE_STEP_FWD    "\xee\x8f\xaa"
#define LUCIDE_ROTATE_CCW  "\xee\x85\x88"
#define LUCIDE_CHEVRON_UP  "\xee\x81\xb0"
#define LUCIDE_CHEVRON_DN  "\xee\x81\xad"
#define LUCIDE_TRASH       "\xee\x86\x8d"
#define LUCIDE_PLUS        "\xee\x84\xbd"

namespace DekiEditor
{

namespace
{
    // Drop a trailing "Modifier" suffix and split CamelCase into spaced words
    // for inspector display. "InitialVelocityModifier" → "Initial Velocity".
    std::string PrettyModifierName(const char* className)
    {
        if (!className) return "";
        std::string s = className;
        constexpr const char* suffix = "Modifier";
        size_t suffixLen = std::strlen(suffix);
        if (s.size() > suffixLen &&
            s.compare(s.size() - suffixLen, suffixLen, suffix) == 0)
        {
            s.resize(s.size() - suffixLen);
        }
        std::string out;
        out.reserve(s.size() + 4);
        for (size_t i = 0; i < s.size(); ++i)
        {
            char c = s[i];
            bool isUpper = (c >= 'A' && c <= 'Z');
            bool prevLower = (i > 0 && s[i-1] >= 'a' && s[i-1] <= 'z');
            bool prevUpper = (i > 0 && s[i-1] >= 'A' && s[i-1] <= 'Z');
            bool nextLower = (i + 1 < s.size() && s[i+1] >= 'a' && s[i+1] <= 'z');
            if (isUpper && i > 0 && (prevLower || (prevUpper && nextLower)))
                out.push_back(' ');
            out.push_back(c);
        }
        return out;
    }

    uint32_t PhaseBadgeColor(int phase)
    {
        if (phase < 10)  return EditorUI::Rgba(220, 150,  60);  // EMIT (orange)
        if (phase < 100) return EditorUI::Rgba(220, 200,  60);  // INIT (yellow)
        if (phase < 200) return EditorUI::Rgba( 60, 180, 220);  // FORCE (cyan)
        return                  EditorUI::Rgba(170, 120, 220);  // LIFE (purple)
    }
    const char* PhaseLabel(int phase)
    {
        if (phase < 10)  return "EMIT";
        if (phase < 100) return "INIT";
        if (phase < 200) return "FORCE";
        return                   "LIFE";
    }

    void DrawPhaseBadge(int phase)
    {
        auto& ui = EditorUI::Get();
        const char* label = PhaseLabel(phase);
        float tw, th;
        ui.MeasureText(label, &tw, &th);
        const float padX = 6.0f, padY = 2.0f;
        const float w = tw + padX * 2.0f, h = th + padY * 2.0f;
        float px, py;
        ui.GetCursorScreenPos(&px, &py);
        ui.DrawRectFilled(px, py, px + w, py + h, PhaseBadgeColor(phase), 3.0f);
        ui.DrawTextAt(0.0f, px + padX, py + padY, EditorUI::Rgba(20, 20, 20), label);
        ui.Dummy(w, h);
    }

    bool HasEmissionSibling(DekiObject* owner)
    {
        const uint32_t emissionType = EmissionModifier::StaticType;
        for (auto* c : owner->GetComponents())
            if (c && c->getType() == emissionType)
                return true;
        return false;
    }
}

class ParticleEmitterCustomEditor : public CustomEditor
{
public:
    const char* GetComponentName() const override { return "ParticleEmitterComponent"; }

    bool WantsInspectorOverride(DekiComponent* /*comp*/) override { return true; }

    void OnEditorUpdate(DekiComponent* comp) override
    {
        auto* emitter = static_cast<ParticleEmitterComponent*>(comp);
        if (!emitter) return;

        // Measure the editor frame delta locally — DekiTime::GetDeltaTimeF()
        // only ticks in Play mode, so in edit mode it always reads 0 and the
        // sim's dt > 0 guard would no-op forever.
        auto now = std::chrono::steady_clock::now();
        auto& last = m_LastTick[emitter];
        float dtSeconds = 0.0f;
        if (last.time_since_epoch().count() != 0)
        {
            using fsec = std::chrono::duration<float>;
            dtSeconds = std::chrono::duration_cast<fsec>(now - last).count();
        }
        last = now;

        // Single-frame Step request fires once even when paused.
        if (m_StepRequested.count(emitter))
        {
            m_StepRequested.erase(emitter);
            emitter->Simulate(1.0f / 60.0f);
        }
        if (!emitter->IsEditorPreviewPlaying()) return;
        emitter->Simulate(dtSeconds * GetSpeed(emitter));
    }

    void OnInspectorGUI(DekiComponent* comp) override
    {
        auto* emitter = static_cast<ParticleEmitterComponent*>(comp);
        if (!emitter) return;

        DekiObject* owner = emitter->GetOwner();
        if (!owner) {
            EditorUI::Get().DrawDefaultInspector();
            return;
        }

        // A particle system without an EmissionModifier sibling spawns nothing
        // — auto-add one the first time the inspector sees this emitter.
        EnsureEmissionExists(emitter, owner);

        EditorUI::Get().DrawDefaultInspector();
        DrawPreviewSection(emitter);
        DrawModulesSection(emitter, owner);
    }

private:
    struct EditorState { float speed = 1.0f; };
    std::unordered_map<ParticleEmitterComponent*, EditorState> m_State;
    std::unordered_map<ParticleEmitterComponent*, bool>        m_StepRequested;
    std::unordered_map<ParticleEmitterComponent*, std::chrono::steady_clock::time_point> m_LastTick;

    float GetSpeed(ParticleEmitterComponent* e)
    {
        auto it = m_State.find(e);
        return (it == m_State.end()) ? 1.0f : it->second.speed;
    }

    void EnsureEmissionExists(ParticleEmitterComponent* emitter, DekiObject* owner)
    {
        if (HasEmissionSibling(owner)) return;
        owner->AddComponent(EmissionModifier::StaticGuid);
        emitter->RefreshModifiers();
        if (auto* prefab = owner->GetOwnerPrefab())
            prefab->InvalidateBehavioursCache();
    }

    void DrawPreviewSection(ParticleEmitterComponent* emitter)
    {
        auto& ui = EditorUI::Get();
        ui.Space();
        ui.Separator();
        ui.TextDisabled("Preview");
        ui.Space();

        // Auto-width buttons so the larger Lucide glyphs fit without truncation.
        const bool playing = emitter->IsEditorPreviewPlaying();
        const char* playLabel = playing ? LUCIDE_PAUSE " Pause" : LUCIDE_PLAY " Play";
        if (ui.Button(playLabel))
            emitter->EditorPreviewSetPlaying(!playing);
        ui.SameLine();
        if (ui.Button(LUCIDE_STEP_FWD " Step"))
            m_StepRequested[emitter] = true;
        ui.SameLine();
        if (ui.Button(LUCIDE_ROTATE_CCW " Restart"))
            emitter->EditorPreviewRestart();
        ui.SameLine();
        ui.AlignTextToFramePadding();
        char aliveBuf[64];
        std::snprintf(aliveBuf, sizeof(aliveBuf), "%d / %d alive",
                      emitter->pool.AliveCount(), emitter->pool.Capacity());
        ui.TextDisabled(aliveBuf);

        // Editor-only preview speed (not document state) -> low-level, no undo.
        float& speed = m_State[emitter].speed;
        ui.PropertyRow("Speed");
        ui.SliderFloat("##preview_speed", &speed, 0.0f, 4.0f, "%.2fx");
    }

    void DrawModulesSection(ParticleEmitterComponent* emitter, DekiObject* owner)
    {
        auto& ui = EditorUI::Get();
        ui.Space();
        ui.Separator();
        ui.TextDisabled("Modules");
        ui.Space();

        std::vector<ParticleModifier*> mods;
        for (auto* c : owner->GetComponents())
            if (auto* m = dynamic_cast<ParticleModifier*>(c))
                mods.push_back(m);
        std::stable_sort(mods.begin(), mods.end(),
            [](ParticleModifier* a, ParticleModifier* b) {
                int pa = a->GetSimulationPhase();
                int pb = b->GetSimulationPhase();
                if (pa != pb) return pa < pb;
                return a->order < b->order;
            });

        DrawModulesList(emitter, owner, mods);
        ui.Space();
        DrawAddModuleButton(emitter, owner);
    }

    void DrawModulesList(ParticleEmitterComponent* emitter, DekiObject* owner,
                         std::vector<ParticleModifier*>& mods)
    {
        const uint32_t emissionType = EmissionModifier::StaticType;

        ParticleModifier* toRemove = nullptr;
        ParticleModifier* swapA = nullptr;
        ParticleModifier* swapB = nullptr;

        auto& ui = EditorUI::Get();
        for (size_t i = 0; i < mods.size(); ++i)
        {
            ParticleModifier* m = mods[i];
            int phase = m->GetSimulationPhase();
            const DekiComponentMeta* meta = ComponentRegistry::Instance().GetMeta(m->getType());
            std::string label = PrettyModifierName(meta ? meta->name : "Modifier");
            const bool isEmission = (m->getType() == emissionType);

            ui.PushID(m);

            // Enable checkbox — emission cannot be disabled (it's required).
            if (isEmission) ui.BeginDisabled();
            ui.Checkbox("##enabled", &m->enabled);
            if (isEmission) {
                m->enabled = true;
                ui.EndDisabled();
            }

            ui.SameLine();
            DrawPhaseBadge(phase);

            ui.SameLine();
            if (!m->enabled) ui.PushStyleColor(EditorUI::Col::Text, EditorUI::Rgba(140, 140, 140));
            ui.AlignTextToFramePadding();
            ui.Text(label.c_str());
            if (!m->enabled) ui.PopStyleColor();

            // Right-align the per-row controls, computed from actual font metrics
            // so the Lucide glyph size doesn't matter.
            float fpx, fpy;  ui.GetFramePadding(&fpx, &fpy);
            float ispx, ispy; ui.GetItemSpacing(&ispx, &ispy);
            auto smallBtnW = [&](const char* lbl) {
                float w, h; ui.MeasureText(lbl, &w, &h);
                return w + fpx * 2.0f;
            };
            float controlsWidth = smallBtnW(LUCIDE_CHEVRON_UP)
                                + smallBtnW(LUCIDE_CHEVRON_DN)
                                + smallBtnW(LUCIDE_TRASH)
                                + ispx * 2.0f;
            float crmx, crmy; ui.GetContentRegionMax(&crmx, &crmy);
            ui.SameLine(crmx - controlsWidth);

            ParticleModifier* prevSamePhase = SamePhaseNeighbor(mods, i, -1, phase);
            if (!prevSamePhase) ui.BeginDisabled();
            if (ui.SmallButton(LUCIDE_CHEVRON_UP)) { swapA = m; swapB = prevSamePhase; }
            if (!prevSamePhase) ui.EndDisabled();

            ui.SameLine();
            ParticleModifier* nextSamePhase = SamePhaseNeighbor(mods, i, +1, phase);
            if (!nextSamePhase) ui.BeginDisabled();
            if (ui.SmallButton(LUCIDE_CHEVRON_DN)) { swapA = m; swapB = nextSamePhase; }
            if (!nextSamePhase) ui.EndDisabled();

            ui.SameLine();
            // Emission cannot be removed — without it the system spawns nothing.
            if (isEmission) ui.BeginDisabled();
            if (ui.SmallButton(LUCIDE_TRASH))
                toRemove = m;
            if (isEmission) ui.EndDisabled();

            if (isEmission && ui.IsItemHovered(true))
                ui.SetTooltip("Emission is required and cannot be removed");

            ui.PopID();
        }

        if (swapA && swapB)
        {
            int oa = swapA->order;
            int ob = swapB->order;
            if (oa == ob) ob = oa + 1;
            swapA->order = ob;
            swapB->order = oa;
            emitter->RefreshModifiers();
        }
        if (toRemove)
        {
            owner->RemoveComponent(toRemove);
            emitter->RefreshModifiers();
            if (auto* prefab = owner->GetOwnerPrefab())
                prefab->InvalidateBehavioursCache();
        }
    }

    static ParticleModifier* SamePhaseNeighbor(const std::vector<ParticleModifier*>& mods,
                                               size_t i, int dir, int phase)
    {
        if (dir < 0)
        {
            for (size_t j = i; j-- > 0;)
            {
                if (mods[j]->GetSimulationPhase() == phase) return mods[j];
                return nullptr;
            }
            return nullptr;
        }
        for (size_t j = i + 1; j < mods.size(); ++j)
        {
            if (mods[j]->GetSimulationPhase() == phase) return mods[j];
            return nullptr;
        }
        return nullptr;
    }

    // -------------------------------------------------------------------
    // Gizmo: visualize the emission shape in the prefab view.
    // OnDrawGizmosSelected fires only when the emitter's owning object is
    // selected — same convention as Unity's "show shape only when selected".
    // -------------------------------------------------------------------
public:
    void OnDrawGizmosSelected(DekiComponent* comp) override
    {
        auto* emitter = static_cast<ParticleEmitterComponent*>(comp);
        if (!emitter) return;
        DekiObject* owner = emitter->GetOwner();
        if (!owner) return;

        // Find sibling EmissionModifier — that's where shape + size live.
        EmissionModifier* em = nullptr;
        const uint32_t emType = EmissionModifier::StaticType;
        for (auto* c : owner->GetComponents())
            if (c && c->getType() == emType) { em = static_cast<EmissionModifier*>(c); break; }
        if (!em) return;

        auto& view = PrefabView::Get();

        const float cx = view.GetScreenX();
        const float cy = view.GetScreenY();
        // Crosshair: fixed-screen accent — scales with editor wheel zoom only.
        // Shape extents (radius/width/height): world meters — scale by
        // GetWorldToScreenScale so they match the rendered emission area.
        const float zoom        = view.GetZoom();
        const float worldToPx   = view.GetWorldToScreenScale();
        const uint32_t color = PrefabView::Rgba(255, 200, 60, 220);

        switch (em->shape)
        {
            case EmitterShapeKind::Point:
            {
                const float k = 6.0f * zoom;
                view.DrawLine(cx - k, cy, cx + k, cy, color, 1.0f);
                view.DrawLine(cx, cy - k, cx, cy + k, color, 1.0f);
                break;
            }
            case EmitterShapeKind::Circle:
            {
                const float r = em->radius * worldToPx;
                if (r > 0.5f)
                    view.DrawCircle(cx, cy, r, color, 1.0f);
                break;
            }
            case EmitterShapeKind::Rect:
            {
                const float halfW = 0.5f * em->width  * worldToPx;
                const float halfH = 0.5f * em->height * worldToPx;
                if (halfW > 0.5f && halfH > 0.5f)
                    view.DrawRect(cx - halfW, cy - halfH, cx + halfW, cy + halfH, color, 1.0f);
                break;
            }
        }
    }

private:
    void DrawAddModuleButton(ParticleEmitterComponent* emitter, DekiObject* owner)
    {
        auto& ui = EditorUI::Get();
        if (ui.Button(LUCIDE_PLUS " Add Module", -1, 0))
            ui.OpenPopup("##add_particle_module");

        if (ui.BeginPopup("##add_particle_module"))
        {
            struct Entry { const DekiComponentMeta* meta; int phase; };
            std::vector<Entry> entries;
            const uint32_t base = ParticleModifier::StaticType;
            for (const DekiComponentMeta* meta : ComponentRegistry::Instance().GetAllComponents())
            {
                if (!meta || meta->isAbstract) continue;
                if (meta->baseTypeId != base) continue;
                if (!meta->createFunc) continue;
                DekiComponent* tmp = meta->createFunc();
                if (!tmp) continue;
                int phase = 0;
                if (auto* pm = dynamic_cast<ParticleModifier*>(tmp))
                    phase = pm->GetSimulationPhase();
                delete tmp;
                entries.push_back({meta, phase});
            }
            std::stable_sort(entries.begin(), entries.end(),
                [](const Entry& a, const Entry& b) { return a.phase < b.phase; });

            if (entries.empty())
            {
                ui.TextDisabled("(no modifier types registered)");
            }
            else
            {
                int lastBucket = -1;
                for (const Entry& e : entries)
                {
                    int bucket = (e.phase < 10) ? 0 : (e.phase < 100) ? 1 : (e.phase < 200) ? 2 : 3;
                    if (bucket != lastBucket)
                    {
                        if (lastBucket >= 0) ui.Separator();
                        ui.TextDisabled(PhaseLabel(e.phase));
                        lastBucket = bucket;
                    }
                    std::string pretty = PrettyModifierName(e.meta->name);
                    if (ui.MenuItem(pretty.c_str()))
                    {
                        owner->AddComponent(e.meta->serializedName);
                        emitter->RefreshModifiers();
                        if (auto* prefab = owner->GetOwnerPrefab())
                            prefab->InvalidateBehavioursCache();
                    }
                }
            }
            ui.EndPopup();
        }
    }
};

REGISTER_EDITOR(ParticleEmitterCustomEditor)

} // namespace DekiEditor

#endif // DEKI_EDITOR
