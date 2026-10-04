#include "ParticleModifierRegistry.h"

namespace DekiParticles
{

ParticleModifierRegistry& ParticleModifierRegistry::Instance()
{
    static ParticleModifierRegistry s_Instance;
    return s_Instance;
}

}  // namespace DekiParticles
