#pragma once

// The particles package: a particle system driven by a graph asset.
//
// The emitter holds a fixed-size particle pool and is drawn as a single
// QuadBlit. Behaviour (emission, shape, gravity, colour, size and rotation over
// lifetime) is authored as a chain of modifier nodes in a ParticleGraph, walked
// once into a flat callback list when the emitter starts. Other packages add
// modifier types by declaring a DEKI_NODE struct in a "Particles/" category and
// registering its ParticleModifierOps, with no change to this package.

// The export macro is in DekiParticlesAPI.h, so component headers can use it
// without including this umbrella header, which includes them.
#include "DekiParticlesAPI.h"

#ifdef DEKI_PACKAGE_PARTICLES

#include "ParticleEmitterComponent.h"
#include "ParticleGraph.h"
#include "ParticleNodes.h"
#include "ParticleModifierRegistry.h"
#include "ParticleSystem.h"

#endif  // DEKI_PACKAGE_PARTICLES
