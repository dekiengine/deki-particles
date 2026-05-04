#include "EmissionModifier.h"
#include "ParticleEmitterComponent.h"
#include "ParticleMath.h"
#include "DekiObject.h"
#include <cmath>

void EmissionModifier::OnAttachToEmitter(ParticleEmitterComponent& /*emitter*/)
{
    m_RateAccumulator = 0.0f;
    m_BurstTimer = 0.0f;
    m_FiredFirstBurst = false;
}

void EmissionModifier::OnEmit(ParticleEmitterComponent& emitter, int i)
{
    using namespace deki_particles;

    // 1. Lifetime — random in [min, max], clamped to a sane minimum.
    float t = emitter.rng.NextFloat01();
    float life = lifetimeMin + (lifetimeMax - lifetimeMin) * t;
    if (life < 0.001f) life = 0.001f;
    emitter.pool.lifetime[i] = life;

    // 2. Spawn position — sample the configured shape in emitter-local space,
    //    then add the emitter's world origin if worldSpace is on so the
    //    particle starts at the emitter's location.
    float ox = 0.0f, oy = 0.0f;
    switch (shape)
    {
        case EmitterShapeKind::Point:
            break;
        case EmitterShapeKind::Circle:
        {
            // Uniform sample inside disc: r = R*sqrt(u), theta = 2pi*v.
            float u = emitter.rng.NextFloat01();
            float v = emitter.rng.NextFloat01();
            float r = radius * std::sqrt(u);
            float theta = 6.2831853f * v;
            ox = r * TrigLUT::Cos(theta);
            oy = r * TrigLUT::Sin(theta);
            break;
        }
        case EmitterShapeKind::Rect:
        {
            ox = (emitter.rng.NextFloat01() - 0.5f) * width;
            oy = (emitter.rng.NextFloat01() - 0.5f) * height;
            break;
        }
    }

    if (emitter.worldSpace && emitter.GetOwner())
    {
        DekiObject* o = emitter.GetOwner();
        emitter.pool.posX[i] = o->GetWorldX() + ox;
        emitter.pool.posY[i] = o->GetWorldY() + oy;
    }
    else
    {
        emitter.pool.posX[i] = ox;
        emitter.pool.posY[i] = oy;
    }
}

void EmissionModifier::OnSimulate(ParticleEmitterComponent& emitter, float dt)
{
    // Continuous emission — accumulator-based so fractional rates work.
    if (emissionRate > 0.0f)
    {
        m_RateAccumulator += emissionRate * dt;
        while (m_RateAccumulator >= 1.0f)
        {
            m_RateAccumulator -= 1.0f;
            if (emitter.Spawn() < 0)
            {
                m_RateAccumulator = 0.0f;  // pool full, drop pending spawns
                break;
            }
        }
    }

    // Burst emission — independent of `emissionRate`. Both can run together
    // for "ambient + occasional puff" effects (the inspector splits them
    // into separate groups so the relationship is visible).
    if (burstCount > 0)
    {
        if (!m_FiredFirstBurst)
        {
            for (int n = 0; n < burstCount; ++n)
                if (emitter.Spawn() < 0) break;
            m_FiredFirstBurst = true;
            m_BurstTimer = 0.0f;
        }
        else if (burstInterval > 0.0f)
        {
            m_BurstTimer += dt;
            while (m_BurstTimer >= burstInterval)
            {
                m_BurstTimer -= burstInterval;
                for (int n = 0; n < burstCount; ++n)
                    if (emitter.Spawn() < 0) break;
            }
        }
    }
}
