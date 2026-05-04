#pragma once

/**
 * @file DekiParticlesModule.h
 * @brief Central header for the Deki Particles Module
 *
 * Modular Unity-style particle system. The emitter holds a fixed-size
 * particle pool and is rendered as a single QuadBlit. Behavior (emission,
 * shape, gravity, color/size/rotation over lifetime, etc.) lives in
 * separate ParticleModifier components attached as siblings to the same
 * DekiObject. External modules can ship new modifier types by registering
 * a DEKI_COMPONENT subclass of ParticleModifier — they appear in the
 * "Add Component" dropdown and the emitter discovers them automatically.
 */

// DLL export macro lives in DekiParticlesAPI.h to avoid circular includes
// when component headers need DEKI_PARTICLES_API but cannot drag in this
// umbrella header (which would re-enter the component headers themselves).
#include "DekiParticlesAPI.h"

#ifdef DEKI_MODULE_PARTICLES

#include "ParticleEmitterComponent.h"
#include "ParticleModifier.h"
#include "ParticleSystem.h"
#include "EmissionModifier.h"
#include "InitialVelocityModifier.h"
#include "InitialRotationModifier.h"
#include "GravityModifier.h"
#include "DragModifier.h"
#include "RotationOverLifetimeModifier.h"
#include "SizeOverLifetimeModifier.h"
#include "ColorOverLifetimeModifier.h"

#endif // DEKI_MODULE_PARTICLES
