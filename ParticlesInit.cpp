#include "ParticlesInit.h"
#include "ParticleGraph.h"
#include "ParticleModifierRegistry.h"

// Global scope, matching ParticlesInit.h - see the comment there.
void DekiParticlesInitSystem()
{
    DekiParticles::RegisterGraphLoader();
    DekiParticles::RegisterModifierLibrary();
}

void DekiParticlesShutdownSystem()
{
}
