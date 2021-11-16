#include "src/journal_manager/replay/active_wb_stripe_replayer.h"
#include "active_wb_stripe_replayer_spy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

#include "test/unit-tests/allocator/i_context_replayer_mock.h"
#include "test/unit-tests/allocator/i_wbstripe_allocator_mock.h"

using ::testing::_;
using testing::NiceMock;
using ::testing::Return;

namespace pos
{
const uint32_t numStripesPerSegment = 64;
const int numUserSegments = 64;
const int numBlksPerChunk = 32;
const int numChunksPerStripe = 4;

const int numWbStripes = 64;
const uint32_t numUserStripes = numStripesPerSegment * numUserSegments;
const uint64_t numBlksPerStripe = numBlksPerChunk * numChunksPerStripe;
const int defaultTestVol = 1;

StripeInfo
GetActiveStripe(int volumeId, int wbufIndex)
{
    StripeId vsid = std::rand() % numUserStripes;
    StripeId wbLsid = std::rand() % numWbStripes;
    StripeId userLsid = vsid;
    BlkOffset lastOffset = std::rand() % (numBlksPerStripe - 1);

    StripeInfo activeStripe(volumeId, vsid, wbLsid, userLsid, lastOffset, wbufIndex);

    return activeStripe;
}

StripeInfo
GetFlushedActiveStripe(int volumeId, int wbufIndex)
{
    StripeInfo activeStripe = GetActiveStripe(volumeId, wbufIndex);
    activeStripe.ResetOffset();

    return activeStripe;
}

VirtualBlkAddr
GetTail(StripeInfo stripe)
{
    VirtualBlkAddr tail = {
        .stripeId = stripe.GetVsid(),
        .offset = stripe.GetLastOffset() + 1};

    return tail;
}

void
ExpectReconstructActiveStripe(MockIWBStripeAllocator* wbStripeAllocator, StripeInfo activeStripe, int retCode)
{
    VirtualBlkAddr tailVsa = {
        .stripeId = activeStripe.GetVsid(),
        .offset = activeStripe.GetLastOffset() + 1};
    EXPECT_CALL(*(wbStripeAllocator),
        ReconstructActiveStripe(activeStripe.GetVolumeId(), activeStripe.GetWbLsid(), tailVsa, _))
        .WillOnce(Return(retCode));
}

void
ExpectRestoreActiveStripeTail(MockIWBStripeAllocator* wbStripeAllocator, StripeInfo activeStripe)
{
    VirtualBlkAddr tail = GetTail(activeStripe);
    EXPECT_CALL(*(wbStripeAllocator),
        SetActiveStripeTail(activeStripe.GetWbIndex(), tail, activeStripe.GetWbLsid()))
        .Times(1);
}

TEST(ActiveWBStripeReplayer, Replay_SingleActiveStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find a single active stripe that is not full
    StripeInfo activeStripe = GetActiveStripe(defaultTestVol, defaultTestVol);

    wbStripeReplayer->Update(activeStripe);

    // Then : Will restore the active stipre tail to this stripe
    ExpectReconstructActiveStripe(wbStripeAllocator, activeStripe, 0);
    ExpectRestoreActiveStripeTail(wbStripeAllocator, activeStripe);

    wbStripeReplayer->Replay();

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_SingleFullActiveStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find a single active stripe that saturated
    StripeInfo fullActiveStripe = GetFlushedActiveStripe(defaultTestVol, defaultTestVol);

    wbStripeReplayer->Update(fullActiveStripe);

    // Then : Will reset the active stripe tail to unmap_vsa
    EXPECT_CALL(*(contextReplayer),
        ResetActiveStripeTail(fullActiveStripe.GetWbIndex()))
        .Times(1);

    wbStripeReplayer->Replay();

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_SingleVolumeMultipleFullStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find several stripes on single volume
    for (int stripe = 0; stripe < 5; stripe++)
    {
        StripeInfo fullStripe = GetFlushedActiveStripe(defaultTestVol, defaultTestVol);

        wbStripeReplayer->Update(fullStripe);
    }

    StripeInfo lastActiveStripe = GetActiveStripe(defaultTestVol, defaultTestVol);
    wbStripeReplayer->Update(lastActiveStripe);

    // Then : Will restore the active stripe tail to the lastActiveStripe
    ExpectReconstructActiveStripe(wbStripeAllocator, lastActiveStripe, 0);
    ExpectRestoreActiveStripeTail(wbStripeAllocator, lastActiveStripe);

    wbStripeReplayer->Replay();

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_MultiVolumeMultipleFullStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find several stripes on multiple volumes
    std::vector<StripeInfo> stripes;
    std::vector<StripeInfo> lastStripesPerVolume;
    for (int volumeId = 1; volumeId < 4; volumeId++)
    {
        for (int stripe = 0; stripe < 5; stripe++)
        {
            StripeInfo fullStripe = GetFlushedActiveStripe(volumeId, volumeId);
            stripes.push_back(fullStripe);
        }

        StripeInfo lastActiveStripe = GetActiveStripe(volumeId, volumeId);
        stripes.push_back(lastActiveStripe);
        lastStripesPerVolume.push_back(lastActiveStripe);
    }

    auto rng = std::default_random_engine{};
    std::shuffle(stripes.begin(), stripes.end(), rng);

    for (auto it = stripes.begin(); it != stripes.end(); ++it)
    {
        wbStripeReplayer->Update(*it);
    }

    // Then : Will be restored active stripe tail to the last stripe updated for each voulme
    for (auto lastActiveStripe : lastStripesPerVolume)
    {
        ExpectReconstructActiveStripe(wbStripeAllocator, lastActiveStripe, 0);
        ExpectRestoreActiveStripeTail(wbStripeAllocator, lastActiveStripe);
    }
    wbStripeReplayer->Replay();

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_SingleVolumeMultipleActiveStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find several unflushed stripes on a single volume
    std::vector<StripeInfo> orphanStripes;
    for (int stripe = 0; stripe < 5; stripe++)
    {
        StripeInfo orphanStripe = GetActiveStripe(defaultTestVol, defaultTestVol);
        wbStripeReplayer->Update(orphanStripe);
        orphanStripes.push_back(orphanStripe);

        StripeInfo fullStripe = GetFlushedActiveStripe(defaultTestVol, defaultTestVol);
        wbStripeReplayer->Update(fullStripe);
    }

    // Then: Reversemap of pending stripes will be reconstructed without first orphan stripe, and restored active stripe tail to the last stripe updated for each voulme
    for (auto it = orphanStripes.begin(); it != orphanStripes.end(); it++)
    {
        if (it->GetVsid() == orphanStripes.back().GetVsid())
        {
            ExpectReconstructActiveStripe(wbStripeAllocator, *it, -1000);
            it = orphanStripes.erase(it);
            break;
        }
        else
        {
            ExpectReconstructActiveStripe(wbStripeAllocator, *it, 0);
        }
    }
    StripeInfo lastActiveStripe = *orphanStripes.rbegin();
    orphanStripes.pop_back();
    ExpectRestoreActiveStripeTail(wbStripeAllocator, lastActiveStripe);

    wbStripeReplayer->Replay();

    // Then: Expected orphan stripes will be matched with pending stripes constructed by replayer
    EXPECT_TRUE(orphanStripes.size() == pendingStripes.size());

    std::sort(orphanStripes.begin(), orphanStripes.end(), [](StripeInfo a, StripeInfo b)
    {
        return (a.GetWbLsid() < b.GetWbLsid());
    });

    std::sort(pendingStripes.begin(), pendingStripes.end(), [](PendingStripe* a, PendingStripe* b)
    {
        return (a->wbLsid < b->wbLsid);
    });

    for (uint32_t index = 0; index < orphanStripes.size(); index++)
    {
        EXPECT_TRUE(orphanStripes[index].GetWbLsid() == pendingStripes[index]->wbLsid);
        EXPECT_TRUE(GetTail(orphanStripes[index]) == pendingStripes[index]->tailVsa);
    }

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_MultiVolumeMultipleActiveStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find several unflushed stripes on a single volume
    std::vector<StripeInfo> orphanStripes;
    std::vector<StripeInfo> lastStripesPerVolume;
    for (int volumeId = 0; volumeId < 5; volumeId++)
    {
        for (int stripe = 0; stripe < 5; stripe++)
        {
            StripeInfo orphanStripe = GetActiveStripe(volumeId, volumeId);
            wbStripeReplayer->Update(orphanStripe);
            orphanStripes.push_back(orphanStripe);

            StripeInfo fullStripe = GetFlushedActiveStripe(volumeId, volumeId);
            wbStripeReplayer->Update(fullStripe);
        }
        StripeInfo lastStripe = GetActiveStripe(volumeId, volumeId);
        wbStripeReplayer->Update(lastStripe);
        lastStripesPerVolume.push_back(lastStripe);
    }

    // Then: Reversemap of pending stripes will be reconstructed, and restored active stripe tail to the last stripe updated for each voulme
    for (auto activeStripe : orphanStripes)
    {
        ExpectReconstructActiveStripe(wbStripeAllocator, activeStripe, 0);
    }
    for (auto lastActiveStripe : lastStripesPerVolume)
    {
        ExpectReconstructActiveStripe(wbStripeAllocator, lastActiveStripe, 0);
        ExpectRestoreActiveStripeTail(wbStripeAllocator, lastActiveStripe);
    }
    wbStripeReplayer->Replay();

    // Then: Expected orphan stripes will be matched with pending stripes constructed by replayer
    EXPECT_TRUE(orphanStripes.size() == pendingStripes.size());

    std::sort(orphanStripes.begin(), orphanStripes.end(), [](StripeInfo a, StripeInfo b)
    {
        return (a.GetWbLsid() < b.GetWbLsid());
    });

    std::sort(pendingStripes.begin(), pendingStripes.end(), [](PendingStripe* a, PendingStripe* b)
    {
        return (a->wbLsid < b->wbLsid);
    });

    for (uint32_t index = 0; index < orphanStripes.size(); index++)
    {
        EXPECT_TRUE(orphanStripes[index].GetWbLsid() == pendingStripes[index]->wbLsid);
        EXPECT_TRUE(GetTail(orphanStripes[index]) == pendingStripes[index]->tailVsa);
    }

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, Replay_testIfFoundPendingActiveStripes)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    NiceMock<MockIWBStripeAllocator>* wbStripeAllocator = new NiceMock<MockIWBStripeAllocator>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;

    ActiveWBStripeReplayer* wbStripeReplayer = new ActiveWBStripeReplayer(contextReplayer, wbStripeAllocator, pendingStripes);

    // When : Find several stripes on single volume
    std::vector<StripeInfo> orphanStripes;
    for (int stripe = 0; stripe < 5; stripe++)
    {
        StripeInfo activeStripe = GetActiveStripe(defaultTestVol, -1);
        wbStripeReplayer->Update(activeStripe);
        if (stripe < 4)
        {
            ExpectReconstructActiveStripe(wbStripeAllocator, activeStripe, 0);
            orphanStripes.push_back(activeStripe);
        }
        else
        {
            ExpectReconstructActiveStripe(wbStripeAllocator, activeStripe, -1000);
        }
    }
    // Then: Pending stripe will be reconstructed, and moved pendingStripes without last stripe which failed to reconstruct
    wbStripeReplayer->Replay();

    // Then: Expected orphan stripes will be matched with pending stripes constructed by replayer
    EXPECT_TRUE(orphanStripes.size() == pendingStripes.size());

    std::sort(orphanStripes.begin(), orphanStripes.end(), [](StripeInfo a, StripeInfo b)
    {
        return (a.GetWbLsid() < b.GetWbLsid());
    });

    std::sort(pendingStripes.begin(), pendingStripes.end(), [](PendingStripe* a, PendingStripe* b)
    {
        return (a->wbLsid < b->wbLsid);
    });

    for (uint32_t index = 0; index < orphanStripes.size(); index++)
    {
        EXPECT_TRUE(orphanStripes[index].GetWbLsid() == pendingStripes[index]->wbLsid);
        EXPECT_TRUE(GetTail(orphanStripes[index]) == pendingStripes[index]->tailVsa);
    }

    delete contextReplayer;
    delete wbStripeAllocator;
}

TEST(ActiveWBStripeReplayer, UpdateRevMaps_MultipleVolumeMultipleActiveStripe)
{
    // Given
    NiceMock<MockIContextReplayer>* contextReplayer = new NiceMock<MockIContextReplayer>;
    EXPECT_CALL(*contextReplayer, GetAllActiveStripeTail()).WillOnce(Return(std::vector<VirtualBlkAddr>(ACTIVE_STRIPE_TAIL_ARRAYLEN, UNMAP_VSA)));
    PendingStripeList pendingStripes;
    ActiveWBStripeReplayerSpy wbStripeReplayer(contextReplayer, nullptr, pendingStripes);

    // When : Find several unflushed stripes on a single volume
    int numVolume = 3;
    int numPendingStripes = 5;
    std::vector<std::pair<StripeInfo, std::map<uint64_t, BlkAddr>>> expectActiveStripes;
    for (int volumeId = 0; volumeId < numVolume; volumeId++)
    {
        for (int index = 0; index < numPendingStripes; index++)
        {
            StripeInfo orphanStripe = GetActiveStripe(volumeId, volumeId);
            wbStripeReplayer.Update(orphanStripe);

            std::map<uint64_t, BlkAddr> expectRevMapInfos;
            expectActiveStripes.push_back(make_pair(orphanStripe, expectRevMapInfos));
        }
        StripeInfo lastStripe = GetActiveStripe(volumeId, volumeId);
        wbStripeReplayer.Update(lastStripe);

        std::map<uint64_t, BlkAddr> expectRevMapInfos;
        expectActiveStripes.push_back(make_pair(lastStripe, expectRevMapInfos));
    }

    // Then : Update each ReverseMapInfos on pending stripes
    BlkAddr startRba = 100;
    for (auto& stripe : expectActiveStripes)
    {
        StripeInfo stripeInfo = stripe.first;
        std::map<uint64_t, BlkAddr>& expectRevMapInfos = stripe.second;
        for (int offset = 0; offset < stripeInfo.GetLastOffset(); offset++)
        {
            wbStripeReplayer.UpdateRevMaps(stripeInfo.GetVolumeId(), stripeInfo.GetVsid(), offset, startRba + offset);
            expectRevMapInfos[offset] = startRba + offset;
        }
    }

    for (auto expectStripe : expectActiveStripes)
    {
        ActiveStripeAddr* actualStripe = wbStripeReplayer.FindStripe(expectStripe.first.GetVolumeId(), expectStripe.first.GetVsid());
        EXPECT_EQ(expectStripe.second, actualStripe->GetRevMap());
    }

    delete contextReplayer;
}
} // namespace pos