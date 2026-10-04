// ParticlePool: the struct-of-arrays store every emitter and modifier indexes.
//
// Its columns are Deki::Buffers, allocated through the engine. These tests
// pin the rules a destructor cannot check: a partial allocation leaves an
// empty pool, not one with five of six columns; an optional column group
// stays disabled as a group; KillSwap moves every column it claims to; and an
// unchanged capacity does not reallocate memory a live emitter is reading.

#include <gtest/gtest.h>

#include <ParticlePool.h>
#include <deki/providers/IMemoryProvider.h>

#include <cstdint>
#include <cstdlib>

// Tests name the package's types unqualified.
using namespace DekiParticles;

using DekiParticles::ParticlePool;

namespace
{

// A memory backend that hands out a fixed number of allocations and then
// refuses. A host test cannot reach the out-of-memory branches any other way:
// capacity is an int, so the largest pool asks for 8 GB a column, which a
// 64-bit desktop simply commits (slowly). On a device the same branches are
// the normal case for a pool a few hundred KB too large.
class FailAfterProvider : public Deki::IMemoryProvider
{
public:
    explicit FailAfterProvider(int allowed)
        : m_Allowed(allowed)
    {
    }

    bool Initialize() override { return true; }
    void Shutdown() override {}

    // One heap, a fixed number of allocations, then refusal.
    bool Serves(Deki::Memory::Region region) const override
    {
        return region == Deki::Memory::Internal || region == Deki::Memory::External;
    }

    void* Allocate(Deki::Memory::Region region, size_t bytes, bool needsDma) override
    {
        (void)needsDma;
        if (!Serves(region) || m_Allowed <= 0)
        {
            return nullptr;
        }
        --m_Allowed;
        return malloc(bytes);
    }

    void Free(Deki::Memory::Region, void* ptr) override { free(ptr); }

    // The budget counts allocations, not bytes, so there is no byte figure to
    // report; a made-up one would appear in the out-of-memory line these tests
    // provoke.
    size_t GetAvailable(Deki::Memory::Region) const override { return 0; }

private:
    int m_Allowed;
};

// Installs that backend for the scope, then removes it. Safe to swap mid-run:
// it wraps the same malloc/free as the default host path, so a block
// allocated under it can be freed after it is gone.
struct ScopedFailingAllocator
{
    explicit ScopedFailingAllocator(int allowed) { Deki::Memory::SetBackend(new FailAfterProvider(allowed)); }
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

    // All six required columns, since every update assumes they exist
    // together.
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
    // Spawn writes only the fields its emit node sets; the rest are read as
    // they are, so they must start at zero.
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
    EXPECT_EQ(pool.Spawn(), -1);  // and nothing can be spawned into it
}

TEST(ParticlePool, SameCapacityDoesNotChurnTheMemory)
{
    // EnsurePoolAllocated calls this whenever maxParticles is touched, which
    // must not move a live emitter's columns from under it.
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
    EXPECT_EQ(pool.Spawn(), -1);  // full, not a wrap or an overrun
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
    EXPECT_FLOAT_EQ(pool.posX[1], 3.0f);       // index 3 moved down
    EXPECT_FLOAT_EQ(pool.lifetime[1], 13.0f);  // every column, not just posX
    EXPECT_FLOAT_EQ(pool.posX[0], 0.0f);       // the others are untouched
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

    EXPECT_EQ(pool.KillSwap(1), -1);  // allocated but not alive
    EXPECT_EQ(pool.KillSwap(-1), -1);
    EXPECT_EQ(pool.KillSwap(99), -1);
    EXPECT_EQ(pool.AliveCount(), 1);  // and none of them killed anything
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

    for (int i = 0; i < 3; ++i)
    {
        ASSERT_EQ(pool.Spawn(), i);
    }
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
    {
        EXPECT_FLOAT_EQ(pool.rotation[i], 0.0f) << i;
    }
}

TEST(ParticlePool, EnsureScaleStartsAtOneNotZero)
{
    // A zero scale would make every particle invisible the frame a Size over
    // Lifetime node attaches.
    ParticlePool pool;
    pool.SetCapacity(8);
    pool.EnsureScale();
    ASSERT_TRUE(pool.HasScale());
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_FLOAT_EQ(pool.scale[i], 1.0f) << i;
    }
}

TEST(ParticlePool, EnsureTintStartsOpaqueWhite)
{
    // A zero tint would make every particle transparent black.
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
    // Every modifier in a chain calls Ensure* in its onAttach, so a second
    // caller must not reallocate what the first is already using.
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
    // All or nothing. With six required columns, five allocating is the
    // dangerous case, because every update indexes all six.
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
    EXPECT_EQ(pool.Spawn(), -1);  // nothing can be spawned into an empty pool
}

TEST(ParticlePool, AnOptionalPairStaysDisabledWhenOnlyHalfOfItFits)
{
    // The spin integrator reads rotation and rotationSpeed together, so having
    // only one of them is not a partial success but a wild read.
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
    // particles that do not rotate are better than none.
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

// --- a package defining its own memory region --------------------------------
// The engine ships only "internal" and "external". A board with other memory
// (RTC RAM that survives deep sleep, a tightly-coupled scratch bank, a
// non-cacheable window for DMA) is described by the package that supports
// the board, with no engine release.
//
// This is a package test, not an engine one, because that is the case worth
// proving: the region is defined in separately compiled package code, and the
// engine carries it without knowing the name.

namespace
{

// The engine names its own regions; a package may add to them or use its own
// prefix, as here. The prefix is only a convention: a region's identity is
// its hashed name.
constexpr Deki::Memory::Region kScratch = Deki::Memory::Region("particles.scratch");

// A board provider that serves the package's region alongside the usual ones.
class ScratchProvider : public Deki::IMemoryProvider
{
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}

    bool Serves(Deki::Memory::Region region) const override
    {
        return region == Deki::Memory::Internal || region == kScratch;
    }

    void* Allocate(Deki::Memory::Region region, size_t bytes, bool) override
    {
        if (!Serves(region))
        {
            return nullptr;
        }
        if (region == kScratch)
        {
            ++scratchCalls;
        }
        return malloc(bytes);
    }

    void Free(Deki::Memory::Region, void* ptr) override { free(ptr); }
    size_t GetAvailable(Deki::Memory::Region region) const override { return Serves(region) ? 64 * 1024 : 0; }

    int scratchCalls = 0;
};

}  // namespace

TEST(PackageDefinedRegion, TheEngineCarriesARegionItHasNeverHeardOf)
{
    auto* provider = new ScratchProvider();
    Deki::Memory::SetBackend(provider);

    Deki::Buffer<uint32_t> b;
    ASSERT_TRUE(b.Allocate(16, kScratch)) << "the provider serves it, so it must succeed";
    EXPECT_EQ(provider->scratchCalls, 1);

    b.Reset();  // and Free routes back to the same region, from the header
    Deki::Memory::SetBackend(nullptr);
}

TEST(PackageDefinedRegion, ItIsAConstantExpressionSoItCostsNothing)
{
    // Usable in a static_assert and a switch label, so a provider can dispatch
    // on it without a runtime lookup or a registration call.
    static_assert(kScratch.id == Deki::HashName("particles.scratch"));
    static_assert(kScratch != Deki::Memory::Internal);
    EXPECT_STREQ(kScratch.name, "particles.scratch");
}
