#pragma once

#include <cstdint>
#include <cstring>
#include <deki/Math.h>
#include <deki/LogSystem.h>
#include <deki/providers/Buffer.h>

namespace DekiParticles
{

/// Struct-of-arrays particle storage, with optional columns allocated on
/// demand.
///
/// Columns always present (allocated when the capacity is set):
///   posX, posY, velX, velY, age, lifetime
///
/// Optional shared columns (allocated on first use through Ensure*):
///   rotation, rotationSpeed, scale, tintR, tintG, tintB, tintA
///
/// A modifier that needs private per-particle data allocates it in its
/// onAttach, in the per-emitter state blob; pool columns are kept for shared
/// state the render path reads.
///
/// A dying particle swaps with the last live one: O(1), and [0, aliveCount)
/// stays dense.
class ParticlePool
{
public:
    ParticlePool() = default;
    ~ParticlePool() { Free(); }

    ParticlePool(const ParticlePool&) = delete;
    ParticlePool& operator=(const ParticlePool&) = delete;

public:
    void SetCapacity(int newCapacity)
    {
        if (newCapacity == m_Capacity)
        {
            return;
        }
        Free();
        m_Capacity = newCapacity;
        m_AliveCount = 0;
        if (newCapacity <= 0)
        {
            return;
        }

        // Time columns (age, lifetime) are in seconds; spatial columns in
        // meters. Internal memory, because every column is walked every frame
        // and external RAM is reached over SPI. Deki::Memory zeroes what it
        // returns.
        posX.Allocate(newCapacity, Deki::Memory::Internal);
        posY.Allocate(newCapacity, Deki::Memory::Internal);
        velX.Allocate(newCapacity, Deki::Memory::Internal);
        velY.Allocate(newCapacity, Deki::Memory::Internal);
        age.Allocate(newCapacity, Deki::Memory::Internal);
        lifetime.Allocate(newCapacity, Deki::Memory::Internal);

        // All or nothing: every update would still index a missing column.
        if (!posX || !posY || !velX || !velY || !age || !lifetime)
        {
            DEKI_LOG_WARNING("ParticlePool: no room for %d particles; the emitter is empty", newCapacity);
            Free();
            m_Capacity = 0;
        }
    }

    int Capacity() const { return m_Capacity; }
    int AliveCount() const { return m_AliveCount; }

    // Claims a new particle slot. Returns -1 if the pool is full.
    int Spawn()
    {
        if (m_AliveCount >= m_Capacity)
        {
            return -1;
        }
        int idx = m_AliveCount++;
        return idx;
    }

    // Kills the particle at idx by moving the last live one into its slot.
    // The caller swaps any optional columns it cares about. Returns the index
    // the last particle moved to (idx), or -1 if idx was past the live range.
    int KillSwap(int idx)
    {
        if (idx < 0 || idx >= m_AliveCount)
        {
            return -1;
        }
        int last = m_AliveCount - 1;
        if (idx != last)
        {
            posX[idx] = posX[last];
            posY[idx] = posY[last];
            velX[idx] = velX[last];
            velY[idx] = velY[last];
            age[idx] = age[last];
            lifetime[idx] = lifetime[last];
            if (m_HasRotation)
            {
                rotation[idx] = rotation[last];
                rotationSpeed[idx] = rotationSpeed[last];
            }
            if (m_HasScale)
            {
                scale[idx] = scale[last];
            }
            if (m_HasTint)
            {
                tintR[idx] = tintR[last];
                tintG[idx] = tintG[last];
                tintB[idx] = tintB[last];
                tintA[idx] = tintA[last];
            }
        }
        m_AliveCount--;
        return idx;
    }

    // Optional columns, allocated on first request. Safe to call repeatedly.
    void EnsureRotation()
    {
        if (m_HasRotation || m_Capacity <= 0)
        {
            return;
        }
        rotation.Allocate(m_Capacity, Deki::Memory::Internal);
        rotationSpeed.Allocate(m_Capacity, Deki::Memory::Internal);
        if (!rotation || !rotationSpeed)
        {
            rotation.Reset();
            rotationSpeed.Reset();
            return;  // stays disabled; particles simply do not rotate
        }
        m_HasRotation = true;
    }
    void EnsureScale()
    {
        if (m_HasScale || m_Capacity <= 0)
        {
            return;
        }
        scale.Allocate(m_Capacity, Deki::Memory::Internal);
        if (!scale)
        {
            return;  // stays disabled; particles keep their size
        }
        for (int i = 0; i < m_Capacity; ++i)
        {
            scale[i] = 1.0f;
        }
        m_HasScale = true;
    }
    void EnsureTint()
    {
        if (m_HasTint || m_Capacity <= 0)
        {
            return;
        }
        tintR.Allocate(m_Capacity, Deki::Memory::Internal);
        tintG.Allocate(m_Capacity, Deki::Memory::Internal);
        tintB.Allocate(m_Capacity, Deki::Memory::Internal);
        tintA.Allocate(m_Capacity, Deki::Memory::Internal);
        if (!tintR || !tintG || !tintB || !tintA)
        {
            tintR.Reset();
            tintG.Reset();
            tintB.Reset();
            tintA.Reset();
            return;  // stays disabled; particles render untinted
        }
        std::memset(tintR.Data(), 255, m_Capacity);
        std::memset(tintG.Data(), 255, m_Capacity);
        std::memset(tintB.Data(), 255, m_Capacity);
        std::memset(tintA.Data(), 255, m_Capacity);
        m_HasTint = true;
    }

    bool HasRotation() const { return m_HasRotation; }
    bool HasScale() const { return m_HasScale; }
    bool HasTint() const { return m_HasTint; }

    // Columns always present, public for tight inner loops. Spatial columns in
    // meters, time columns (age, lifetime) in seconds.
    Deki::Buffer<float> posX;
    Deki::Buffer<float> posY;
    Deki::Buffer<float> velX;
    Deki::Buffer<float> velY;
    Deki::Buffer<float> age;
    Deki::Buffer<float> lifetime;

    // Optional columns, empty until their Ensure* is called. rotation is in
    // radians (engine convention).
    Deki::Buffer<float> rotation;       // radians
    Deki::Buffer<float> rotationSpeed;  // radians/sec
    Deki::Buffer<float> scale;
    Deki::Buffer<uint8_t> tintR;
    Deki::Buffer<uint8_t> tintG;
    Deki::Buffer<uint8_t> tintB;
    Deki::Buffer<uint8_t> tintA;

private:
    int m_Capacity = 0;
    int m_AliveCount = 0;
    bool m_HasRotation = false;
    bool m_HasScale = false;
    bool m_HasTint = false;

    void Free()
    {
        posX.Reset();
        posY.Reset();
        velX.Reset();
        velY.Reset();
        age.Reset();
        lifetime.Reset();
        rotation.Reset();
        rotationSpeed.Reset();
        scale.Reset();
        tintR.Reset();
        tintG.Reset();
        tintB.Reset();
        tintA.Reset();
        m_HasRotation = m_HasScale = m_HasTint = false;
        m_AliveCount = 0;
    }
};

}  // namespace DekiParticles
