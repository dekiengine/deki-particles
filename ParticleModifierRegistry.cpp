#include "ParticleModifierRegistry.h"

namespace DekiParticles
{

ParticleModifierRegistry& ParticleModifierRegistry::Instance()
{
    static ParticleModifierRegistry instance;
    return instance;
}

}  // namespace DekiParticles
