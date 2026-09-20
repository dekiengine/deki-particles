# Deki Particles

Docs: https://dekiengine.github.io/deki-particles/ (components and properties, generated from the code)

Particle system for the Deki Engine: emission shapes (point/circle/rect), gravity, drag, initial velocity, and lifetime/size/color/rotation modifiers.

An effect is a **Particle Effect** asset, authored in the editor's Node Graph
window: an Emitter node, then modifier nodes wired in the order they run.
`ParticleEmitterComponent` points at the asset and walks it once on start, so
the per-frame cost is just the modifiers you wired. One graph can drive any
number of emitters.

Part of [Deki Engine](https://github.com/dekiengine/deki-engine).

## Namespace

Types live in `DekiParticles`. Scene files store the qualified name, and so does code:

```cpp
using namespace DekiParticles;
obj->AddComponent<SomeComponent>();
```

Scenes saved before 0.16.0 used bare names and still load; saving writes the current one.

## Install

Package Manager in the Deki Editor, or `DekiEditor --packages-add deki-particles <project>`.

## Units

Positions and velocities are world metres, like everywhere else. Node speeds
are m/s, radii are metres. The sprite draws at its own `pixelsPerMeter`, so
scale 1 means "the size the art was authored at".

Before September 2026 the emitter composited at one pixel per metre, so a
16 px sprite covered 16 m on screen. Old scenes now show their particles at
the authored size instead. If you wanted the old look, scale them up in the
graph's size node.

## Adding your own modifier

From any package or the project DLL, with no change to deki-particles:

1. Declare a `DEKI_NODE` struct in a category starting `Particles/` (see `ParticleNodes.h`).
2. Register its `ParticleModifierOps` with `REGISTER_PARTICLE_MODIFIER` (see `ParticleModifierLibrary.cpp`). Anything the modifier remembers between frames goes in the ops' state blob, never in the struct: the struct belongs to the shared asset.
3. Register the node type from your package entry, the way `DekiParticles_RegisterGraphTypes` does.

## Dependencies

| Dependency | Type |
|---|---|
| `deki-nodegraph` | Deki package |
| `deki-rendering` | Deki package |
| `deki-2d` | Deki package |

## License

Apache 2.0. See [LICENSE](LICENSE).
