#pragma once

#include <vector>
#include "DekiParticlesAPI.h"

namespace DekiParticles
{

class ParticleEmitterComponent;

/// A small registry of live particle emitters, shaped like AnimationSystem.
/// It exists for cleanup: when the editor stops Play, it clears all emitter
/// pointers so stale ones do not reach the next session.
///
/// Simulation runs in ParticleEmitterComponent::Update(); this class does not
/// visit emitters each frame.
class DEKI_PARTICLES_API ParticleSystem
{
public:
    static ParticleSystem& GetInstance();

    void RegisterEmitter(ParticleEmitterComponent* emitter);
    void UnregisterEmitter(ParticleEmitterComponent* emitter);
    void ClearAll();

    int EmitterCount() const { return (int)m_Emitters.size(); }

private:
    ParticleSystem() = default;
    std::vector<ParticleEmitterComponent*> m_Emitters;
};

}  // namespace DekiParticles
