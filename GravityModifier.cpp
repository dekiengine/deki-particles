#include "GravityModifier.h"
#include "ParticleEmitterComponent.h"

void GravityModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    int n = emitter.pool.AliveCount();
    float* vx = emitter.pool.velX;
    float* vy = emitter.pool.velY;
    float dvx = gravityX * dt;
    float dvy = gravityY * dt;
    for (int i = 0; i < n; ++i)
    {
        vx[i] += dvx;
        vy[i] += dvy;
    }
}
