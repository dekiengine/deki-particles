#include "GravityModifier.h"
#include "ParticleEmitterComponent.h"

void GravityModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    int n = emitter.pool.AliveCount();
    float* vx = emitter.pool.velX;
    float* vy = emitter.pool.velY;
    // dt is float seconds (engine time domain) — convert once before the hot loop.
    float dtN = static_cast<float>(dt);
    float dvx = ((gravityX) * (dtN));
    float dvy = ((gravityY) * (dtN));
    for (int i = 0; i < n; ++i)
    {
        vx[i] = ((vx[i]) + (dvx));
        vy[i] = ((vy[i]) + (dvy));
    }
}
