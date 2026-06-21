#include "InitialVelocityModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"
#include <cmath>

void InitialVelocityModifier::OnEmit(ParticleEmitterComponent& emitter, int i)
{
    float speed = emitter.rng.NextFloatRange(speedMin, speedMax);
    float angle = emitter.rng.NextFloatRange(angleMin, angleMax);
    // Angle in radians (engine convention).
    emitter.pool.velX[i] = speed * std::cos(angle);
    emitter.pool.velY[i] = speed * std::sin(angle);
}
