#include "InitialVelocityModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"

void InitialVelocityModifier::OnEmit(ParticleEmitterComponent& emitter, int i)
{
    using namespace deki_particles;
    float spd = emitter.rng.NextFloatRange(speedMin, speedMax);
    float ang = DegToRad(emitter.rng.NextFloatRange(angleMin, angleMax));
    emitter.pool.velX[i] = spd * TrigLUT::Cos(ang);
    emitter.pool.velY[i] = spd * TrigLUT::Sin(ang);
}
