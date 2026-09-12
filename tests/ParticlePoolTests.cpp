// ParticlePool - the struct-of-arrays store every emitter and modifier indexes.
//
// The columns moved from raw `new float[]` to Deki::Buffer so they allocate
// through the engine and free themselves. That conversion is the reason this
// suite exists: the pool's rules are all invariants a destructor cannot check
// on its own - that a partial allocation leaves an EMPTY pool rather than one
// with five of six columns, that an optional column group stays disabled as a
// group, that KillSwap moves every column it claims to, and that a capacity
// that has not changed does not churn the memory a live emitter is reading.

#include <gtest/gtest.h>

#include <ParticlePool.h>
#include <deki/providers/IMemoryProvider.h>

#include <cstdint>
#include <cstdlib>

using deki_particles::ParticlePool;

namespace
{

// A memory backend that hands out a fixed number of allocations and then
// refuses. The out-of-memory branches below cannot be reached any other way
// from a host test: capacity is an int, so the largest pool asks for 8 GB a
// column, which a 64-bit desktop simply commits (it took 22 seconds and
// succeeded before this existed). On the device the same branches are the
// normal case for a pool a few hundred KB too large.
class FailAfterProvider : public Deki::IMemoryProvider
{
public:
    explicit FailAfterProvider(int allowed) : m_Allowed(allowed) {}

    bool Initialize() override { return true; }
    void Shutdown() override {}

    bool HasExternalRAM() const override { return false; }
    size_t GetTotalExternalRAM() const override { return 0; }
    size_t GetAvailableExternalRAM() const override { return 0; }
    void* AllocateExternal(size_t) override { return nullptr; }
    void FreeExternal(void*) override {}
    bool IsExternalPointer(void*) const override { return false; }

    void* AllocateInternal(size_t size) override
    {
        if (m_Allowed <= 0)
            return nullptr;
        --m_Allowed;
        return malloc(size);
    }
    void FreeInternal(void* ptr) override { free(ptr); }

private:
    int m_Allowed;
};

// Installs that backend for the scope and takes it back out again. Safe to
// swap mid-run because it wraps the same malloc/free the default host path
// uses, so a block allocated under it can be freed after it is gone.
struct ScopedFailingAllocator
{
    explicit ScopedFailingAllocator(int allowed)
    {
        Deki::Memory::SetBackend(new FailAfterProvider(allowed));
    }
    ~ScopedFailingAllocator() { Deki::Memory::SetBackend(nullptr); }
};

}  // namespace

TEST(ParticlePool, DefaultIsEmpty)
{
    ParticlePool pool;
    EXPECT_EQ(pool.Capacity(), 0);
    EXPECT_EQ(pool.AliveCount(), 0);
    EXPECT_FALSE(pool.HasRotation());
    EXPECT_FALSE(pool.HasScale());
    EXPECT_FALSE(pool.HasTint());
}

TEST(ParticlePool, SetCapacityAllocatesEveryRequiredColumn)
{
    ParticlePool pool;
    pool.SetCapacity(64);
    ASSERT_EQ(pool.Capacity(), 64);

    // All six required columns, or the pool would be indexed past its end by
    // an update that assumes they exist together.
    EXPECT_TRUE(static_cast<bool>(pool.posX));
    EXPECT_TRUE(static_cast<bool>(pool.posY));
    EXPECT_TRUE(static_cast<bool>(pool.velX));
    EXPECT_TRUE(static_cast<bool>(pool.velY));
    EXPECT_TRUE(static_cast<bool>(pool.age));
    EXPECT_TRUE(static_cast<bool>(pool.lifetime));
    EXPECT_EQ(pool.posX.Count(), 64u);
}

TEST(ParticlePool, RequiredColumnsStartZeroed)
{
    // Spawn only writes the fields its emit node sets; the rest are read as
    // they are. The columns used to be `new float[n]{}` for this reason.
    ParticlePool pool;
    pool.SetCapacity(32);
    ASSERT_EQ(pool.Capacity(), 32);
    for (int i = 0; i < 32; ++i)
    {
        EXPECT_FLOAT_EQ(pool.posX[i], 0.0f) << "posX " << i;
        EXPECT_FLOAT_EQ(pool.velY[i], 0.0f) << "velY " << i;
        EXPECT_FLOAT_EQ(pool.age[i], 0.0f) << "age " << i;
        EXPECT_FLOAT_EQ(pool.lifetime[i], 0.0f) << "lifetime " << i;
    }
}

TEST(ParticlePool, ZeroAndNegativeCapacityAllocateNothing)
{
    ParticlePool pool;
    pool.SetCapacity(0);
    EXPECT_EQ(pool.Capacity(), 0);
    EXPECT_FALSE(static_cast<bool>(pool.posX));

    pool.SetCapacity(-5);
    EXPECT_FALSE(static_cast<bool>(pool.posX));
    EXPECT_EQ(pool.Spawn(), -1);      // and nothing can be spawned into it
}

TEST(ParticlePool, SameCapacityDoesNotChurnTheMemory)
{
    // EnsurePoolAllocated calls this whenever maxParticles is touched, which
    // on a live emitter must not move the columns out from under it.
    ParticlePool pool;
    pool.SetCapacity(16);
    const float* raw = pool.posX.Data();
    ASSERT_NE(raw, nullptr);

    pool.SetCapacity(16);
    EXPECT_EQ(pool.posX.Data(), raw);
}

TEST(ParticlePool, ResizingReleasesTheOldColumnsAndResetsAlive)
{
    ParticlePool pool;
    pool.SetCapacity(8);
    ASSERT_EQ(pool.Spawn(), 0);
    ASSERT_EQ(pool.Spawn(), 1);
    ASSERT_EQ(pool.AliveCount(), 2);

    pool.SetCapacity(4);
    EXPECT_EQ(pool.Capacity(), 4);
    EXPECT_EQ(pool.AliveCount(), 0);  // the particles they described are gone
    EXPECT_EQ(pool.posX.Count(), 4u);
}

TEST(ParticlePool, ResizingDisablesTheOptionalColumnGroups)
{
    // They were sized for the old capacity; a modifier's onAttach asks for
    // them again after a resize.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureRotation();
    pool.EnsureScale();
    pool.EnsureTint();
    ASSERT_TRUE(pool.HasRotation());
    ASSERT_TRUE(pool.HasScale());
    ASSERT_TRUE(pool.HasTint());

    pool.SetCapacity(16);
    EXPECT_FALSE(pool.HasRotation());
    EXPECT_FALSE(pool.HasScale());
    EXPECT_FALSE(pool.HasTint());
    EXPECT_FALSE(static_cast<bool>(pool.rotation));
    EXPECT_FALSE(static_cast<bool>(pool.scale));
    EXPECT_FALSE(static_cast<bool>(pool.tintA));
}

TEST(ParticlePool, SpawnHandsOutDenseIndicesThenRefuses)
{
    ParticlePool pool;
    pool.SetCapacity(3);
    EXPECT_EQ(pool.Spawn(), 0);
    EXPECT_EQ(pool.Spawn(), 1);
    EXPECT_EQ(pool.Spawn(), 2);
    EXPECT_EQ(pool.Spawn(), -1);      // full, not a wrap or an overrun
    EXPECT_EQ(pool.AliveCount(), 3);
}

TEST(ParticlePool, KillSwapMovesTheLastAliveIntoTheHole)
{
    ParticlePool pool;
    pool.SetCapacity(4);
    for (int i = 0; i < 4; ++i)
    {
        ASSERT_EQ(pool.Spawn(), i);
        pool.posX[i] = static_cast<float>(i);
        pool.lifetime[i] = static_cast<float>(10 + i);
    }

    EXPECT_EQ(pool.KillSwap(1), 1);
    EXPECT_EQ(pool.AliveCount(), 3);
    EXPECT_FLOAT_EQ(pool.posX[1], 3.0f);        // index 3 moved down
    EXPECT_FLOAT_EQ(pool.lifetime[1], 13.0f);   // every column, not just posX
    EXPECT_FLOAT_EQ(pool.posX[0], 0.0f);        // the others are untouched
    EXPECT_FLOAT_EQ(pool.posX[2], 2.0f);
}

TEST(ParticlePool, KillSwapOfTheLastAliveJustShrinks)
{
    ParticlePool pool;
    pool.SetCapacity(2);
    ASSERT_EQ(pool.Spawn(), 0);
    ASSERT_EQ(pool.Spawn(), 1);
    pool.posX[1] = 7.0f;

    EXPECT_EQ(pool.KillSwap(1), 1);
    EXPECT_EQ(pool.AliveCount(), 1);
    EXPECT_FLOAT_EQ(pool.posX[1], 7.0f);  // no self-swap needed
}

TEST(ParticlePool, KillSwapRejectsIndicesOutsideTheAliveRange)
{
    ParticlePool pool;
    pool.SetCapacity(4);
    ASSERT_EQ(pool.Spawn(), 0);

    EXPECT_EQ(pool.KillSwap(1), -1);   // allocated but not alive
    EXPECT_EQ(pool.KillSwap(-1), -1);
    EXPECT_EQ(pool.KillSwap(99), -1);
    EXPECT_EQ(pool.AliveCount(), 1);   // and none of them killed anything
}

TEST(ParticlePool, KillSwapCarriesTheOptionalColumnsToo)
{
    ParticlePool pool;
    pool.SetCapacity(3);
    pool.EnsureRotation();
    pool.EnsureScale();
    pool.EnsureTint();
    ASSERT_TRUE(pool.HasRotation());
    ASSERT_TRUE(pool.HasScale());
    ASSERT_TRUE(pool.HasTint());

    for (int i = 0; i < 3; ++i) ASSERT_EQ(pool.Spawn(), i);
    pool.rotation[2] = 1.5f;
    pool.rotationSpeed[2] = 2.5f;
    pool.scale[2] = 3.5f;
    pool.tintR[2] = 10;
    pool.tintG[2] = 20;
    pool.tintB[2] = 30;
    pool.tintA[2] = 40;

    ASSERT_EQ(pool.KillSwap(0), 0);
    EXPECT_FLOAT_EQ(pool.rotation[0], 1.5f);
    EXPECT_FLOAT_EQ(pool.rotationSpeed[0], 2.5f);
    EXPECT_FLOAT_EQ(pool.scale[0], 3.5f);
    EXPECT_EQ(pool.tintR[0], 10);
    EXPECT_EQ(pool.tintG[0], 20);
    EXPECT_EQ(pool.tintB[0], 30);
    EXPECT_EQ(pool.tintA[0], 40);
}

TEST(ParticlePool, EnsureRotationAllocatesBothColumnsOfThePair)
{
    // rotation without rotationSpeed would be indexed by the spin integrator.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureRotation();
    ASSERT_TRUE(pool.HasRotation());
    EXPECT_EQ(pool.rotation.Count(), 8u);
    EXPECT_EQ(pool.rotationSpeed.Count(), 8u);
    for (int i = 0; i < 8; ++i)
        EXPECT_FLOAT_EQ(pool.rotation[i], 0.0f) << i;
}

TEST(ParticlePool, EnsureScaleStartsAtOneNotZero)
{
    // Zero-initialised scale would make every particle invisible the frame a
    // Size over Lifetime node attaches.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureScale();
    ASSERT_TRUE(pool.HasScale());
    for (int i = 0; i < 8; ++i)
        EXPECT_FLOAT_EQ(pool.scale[i], 1.0f) << i;
}

TEST(ParticlePool, EnsureTintStartsOpaqueWhite)
{
    // Zero-initialised tint would multiply every particle to transparent black.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureTint();
    ASSERT_TRUE(pool.HasTint());
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(pool.tintR[i], 255) << i;
        EXPECT_EQ(pool.tintG[i], 255) << i;
        EXPECT_EQ(pool.tintB[i], 255) << i;
        EXPECT_EQ(pool.tintA[i], 255) << i;
    }
}

TEST(ParticlePool, EnsureIsIdempotentAndKeepsTheWrittenValues)
{
    // Every modifier in a chain calls Ensure* in its onAttach, so the second
    // caller must not reallocate over what the first one is already using.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureScale();
    pool.scale[3] = 9.0f;
    const float* raw = pool.scale.Data();

    pool.EnsureScale();
    EXPECT_EQ(pool.scale.Data(), raw);
    EXPECT_FLOAT_EQ(pool.scale[3], 9.0f);
}

TEST(ParticlePool, EnsureBeforeCapacityAllocatesNothing)
{
    // A modifier's onAttach can run before maxParticles is known.
    ParticlePool pool;
    pool.EnsureRotation();
    pool.EnsureScale();
    pool.EnsureTint();
    EXPECT_FALSE(pool.HasRotation());
    EXPECT_FALSE(pool.HasScale());
    EXPECT_FALSE(pool.HasTint());
    EXPECT_FALSE(static_cast<bool>(pool.rotation));
}

TEST(ParticlePool, OutOfMemoryLeavesThePoolEmptyRatherThanPartial)
{
    // All or nothing. There are six required columns; five of them allocating
    // is the dangerous case, because every update indexes all six.
    ScopedFailingAllocator oom(5);

    ParticlePool pool;
    pool.SetCapacity(64);

    EXPECT_EQ(pool.Capacity(), 0);
    EXPECT_EQ(pool.AliveCount(), 0);
    EXPECT_FALSE(static_cast<bool>(pool.posX));
    EXPECT_FALSE(static_cast<bool>(pool.posY));
    EXPECT_FALSE(static_cast<bool>(pool.velX));
    EXPECT_FALSE(static_cast<bool>(pool.velY));
    EXPECT_FALSE(static_cast<bool>(pool.age));
    EXPECT_FALSE(static_cast<bool>(pool.lifetime));
    EXPECT_EQ(pool.Spawn(), -1);   // nothing can be spawned into an empty pool
}

TEST(ParticlePool, AnOptionalPairStaysDisabledWhenOnlyHalfOfItFits)
{
    // rotation and rotationSpeed are read together by the spin integrator, so
    // one of the two is not a partial success, it is a wild read.
    ParticlePool pool;
    pool.SetCapacity(64);
    ASSERT_EQ(pool.Capacity(), 64);

    ScopedFailingAllocator oom(1);
    pool.EnsureRotation();

    EXPECT_FALSE(pool.HasRotation());
    EXPECT_FALSE(static_cast<bool>(pool.rotation));
    EXPECT_FALSE(static_cast<bool>(pool.rotationSpeed));
}

TEST(ParticlePool, TheTintGroupStaysDisabledWhenOnlySomeChannelsFit)
{
    ParticlePool pool;
    pool.SetCapacity(64);
    ASSERT_EQ(pool.Capacity(), 64);

    ScopedFailingAllocator oom(2);  // R and G fit, B and A do not
    pool.EnsureTint();

    EXPECT_FALSE(pool.HasTint());
    EXPECT_FALSE(static_cast<bool>(pool.tintR));
    EXPECT_FALSE(static_cast<bool>(pool.tintG));
    EXPECT_FALSE(static_cast<bool>(pool.tintB));
    EXPECT_FALSE(static_cast<bool>(pool.tintA));
}

TEST(ParticlePool, ARequiredPoolThatFitsIsStillUsableAfterAnOptionalOneDoesNot)
{
    // The emitter carries on without the feature rather than going empty:
    // particles that do not rotate still beat no particles.
    ParticlePool pool;
    pool.SetCapacity(8);
    ASSERT_EQ(pool.Capacity(), 8);

    {
        ScopedFailingAllocator oom(0);
        pool.EnsureScale();
        EXPECT_FALSE(pool.HasScale());
    }

    EXPECT_EQ(pool.Spawn(), 0);
    pool.posX[0] = 4.0f;
    EXPECT_FLOAT_EQ(pool.posX[0], 4.0f);
}
