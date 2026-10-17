// The text the particle effect node types show in the Node Graph window:
// their names, descriptions, pin labels, menu groups, field names and enum
// values.
//
// The node types are declared in ParticleNodes.h, which is compiled for
// devices and cannot mark text for translation, so the text is listed here,
// where tools/ci/check_translations.py finds it and editor/lang/ja.json
// translates it. The Node Graph window passes it through Tr where it draws it.
// Keep this list in step with that header.

#ifdef DEKI_EDITOR

#include <deki-editor/Localization.h>

namespace DekiEditor
{
namespace
{

[[maybe_unused]] constexpr const char* kParticleNodeText[] = {
    // Emitter
    TrMark("Emitter"),
    TrMark("Where the chain starts. Wire it to the first modifier."),
    TrMark("chain"),
    TrMark("Flow"),
    // Emission
    TrMark("Emission"),
    TrMark("How often particles spawn, where, and how long they live."),
    TrMark("in"),
    TrMark("next"),
    TrMark("Enabled"),
    TrMark("Lifetime Min"),
    TrMark("Lifetime"),
    TrMark("Lifetime Max"),
    TrMark("Emission Rate"),
    TrMark("Continuous"),
    TrMark("Burst Count"),
    TrMark("Burst"),
    TrMark("Burst Interval"),
    TrMark("Shape"),
    TrMark("Point"),
    TrMark("Circle"),
    TrMark("Rect"),
    TrMark("Radius"),
    TrMark("Width"),
    TrMark("Height"),
    // Initial Velocity
    TrMark("Initial Velocity"),
    TrMark("Gives each new particle a starting speed and direction."),
    TrMark("Initial"),
    TrMark("Speed Min"),
    TrMark("Speed Max"),
    TrMark("Angle Min"),
    TrMark("Angle Max"),
    // Initial Rotation
    TrMark("Initial Rotation"),
    TrMark("Gives each new particle a starting angle and spin rate."),
    TrMark("Rotation Min"),
    TrMark("Rotation Max"),
    TrMark("Spin Speed Min"),
    TrMark("Spin Speed Max"),
    // Gravity
    TrMark("Gravity"),
    TrMark("Pulls particles with a constant acceleration."),
    TrMark("Forces"),
    TrMark("Gravity X"),
    TrMark("Gravity Y"),
    // Drag
    TrMark("Drag"),
    TrMark("Slows particles down over time."),
    // Size over Lifetime
    TrMark("Size over Lifetime"),
    TrMark("Grows or shrinks a particle as it ages."),
    TrMark("Over Lifetime"),
    TrMark("Size At 0"),
    TrMark("Size At 1"),
    // Color over Lifetime
    TrMark("Color over Lifetime"),
    TrMark("Fades a particle's color and alpha as it ages."),
    TrMark("Color At 0"),
    TrMark("Color At 1"),
    // Rotation over Lifetime
    TrMark("Rotation over Lifetime"),
    TrMark("Ramps a particle's spin rate as it ages."),
    TrMark("Spin Speed At 0"),
    TrMark("Spin Speed At 1"),
};

}  // namespace
}  // namespace DekiEditor

#endif  // DEKI_EDITOR
