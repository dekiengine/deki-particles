#include "DragModifier.h"
#include "ParticleEmitterComponent.h"

void DragModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    float k = 1.0f - drag * dt;
    if (k < 0.0f) k = 0.0f;
    int n = emitter.pool.AliveCount();
    float* vx = emitter.pool.velX;
    float* vy = emitter.pool.velY;
    for (int i = 0; i < n; ++i)
    {
        vx[i] *= k;
        vy[i] *= k;
    }
}
