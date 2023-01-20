#include "gtest/gtest.h"
#include <iostream>

#include "test/integration-tests/journal/fixture/journal_manager_test_fixture.h"
#include "test/integration-tests/journal/utils/used_offset_calculator.h"
#include "src/journal_manager/log/log_event.h"
#include "src/allocator/include/allocator_const.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Return;

namespace pos
{
class ReplayLogBufferIntegrationTest : public JournalManagerTestFixture, public ::testing::Test
{
public:
    ReplayLogBufferIntegrationTest(void);
    virtual ~ReplayLogBufferIntegrationTest(void) = default;

protected:
    virtual void SetUp(void);
    virtual void TearDown(void);

    uint64_t _CalculateLogBufferSize(uint32_t numStripes, uint32_t numBlockMapLogsInStripe);
    void _IncreaseOffsetIfOverMpageSize(uint64_t& nextOffset, uint64_t logSize);
};

ReplayLogBufferIntegrationTest::ReplayLogBufferIntegrationTest(void)
:JournalManagerTestFixture(GetLogFileName())
{
}

void
ReplayLogBufferIntegrationTest::SetUp(void)
{
}

void
ReplayLogBufferIntegrationTest::TearDown(void)
{
}

void
ReplayLogBufferIntegrationTest::_IncreaseOffsetIfOverMpageSize(uint64_t& nextOffset, uint64_t logSize)
{
    uint64_t currentMetaPage = nextOffset / testInfo->metaPageSize;
    uint64_t endMetaPage = (nextOffset + logSize - 1) / testInfo->metaPageSize;
    if (currentMetaPage != endMetaPage)
    {
        nextOffset = endMetaPage * testInfo->metaPageSize;
    }
}

uint64_t
ReplayLogBufferIntegrationTest::_CalculateLogBufferSize(uint32_t numStripes, uint32_t numBlockMapLogsInStripe)
{
    uint64_t nextOffset = 0;

    for (uint32_t stripeIndex = 0; stripeIndex < numStripes; stripeIndex++)
    {
        for (uint32_t blockIndex = 0; blockIndex < numBlockMapLogsInStripe; blockIndex++)
        {
            _IncreaseOffsetIfOverMpageSize(nextOffset, sizeof(BlockWriteDoneLog));
            nextOffset += sizeof(BlockWriteDoneLog);
        }
        _IncreaseOffsetIfOverMpageSize(nextOffset, sizeof(StripeMapUpdatedLog));
        nextOffset += sizeof(StripeMapUpdatedLog);
    }

    nextOffset += sizeof(LogGroupFooter);
    return nextOffset;
}

TEST_F(ReplayLogBufferIntegrationTest, ReplayFullLogBuffer)
{
    POS_TRACE_DEBUG(9999, "ReplayLogBufferIntegrationTest::ReplaySeveralLogGroup");

    JournalConfigurationBuilder builder(testInfo);
    builder.SetJournalEnable(true)
        ->SetLogBufferSize(16 * 1024);

    InitializeJournal(builder.Build());
    SetTriggerCheckpoint(false);

    BlkAddr rba = std::rand() % testInfo->defaultTestVolSizeInBlock;
    StripeId vsid = std::rand() % testInfo->numUserStripes;
    StripeTestFixture stripe(vsid, testInfo->defaultTestVol);

    UsedOffsetCalculator usedOffset(journal, logBufferSize - sizeof(LogGroupFooter));
    uint64_t startOffset = 0;
    while (usedOffset.CanBeWritten(sizeof(BlockWriteDoneLog)) == true)
    {
        writeTester->WriteOverwrittenBlockLogs(stripe, rba, startOffset++, 1);
    }

    writeTester->WaitForAllLogWriteDone();
    SimulateSPORWithoutRecovery();

    replayTester->ExpectReturningUnmapStripes();
    replayTester->ExpectReplayOverwrittenBlockLog(stripe);

    VirtualBlks writtenLastBlock = stripe.GetBlockMapList().back().second;
    VirtualBlkAddr tail = ReplayTestFixture::GetNextBlock(writtenLastBlock);
    replayTester->ExpectReplayUnflushedActiveStripe(tail, stripe);

    EXPECT_TRUE(journal->DoRecoveryForTest() == 0);
}

TEST_F(ReplayLogBufferIntegrationTest, ReplayCirculatedLogBuffer)
{
    POS_TRACE_DEBUG(9999, "ReplayLogBufferIntegrationTest::ReplayCirculatedLogBuffer");

    JournalConfigurationBuilder builder(testInfo);
    builder.SetJournalEnable(true)
        ->SetLogBufferSize(16 * 1024);

    InitializeJournal(builder.Build());
    SetTriggerCheckpoint(false);

    // Write dummy logs to the first log group (to be cleared by checkpoint later)
    writeTester->WriteLogsWithSize(logGroupSize - sizeof(LogGroupFooter));

    // Write logs to fill log buffer, and start checkpoint to clear the first log group
    StripeId currentVsid = 0;

    UsedOffsetCalculator usedOffset(journal, logBufferSize);
    std::list<StripeTestFixture> writtenLogs;
    while (1)
    {
        StripeTestFixture stripe(currentVsid++, testInfo->defaultTestVol);
        writeTester->GenerateLogsForStripe(stripe, 0, testInfo->numBlksPerStripe);

        uint32_t logSize = sizeof(BlockWriteDoneLog) * stripe.GetBlockMapList().size()
            + sizeof(StripeMapUpdatedLog);
        if (usedOffset.CanBeWritten(logSize) == true)
        {
            writeTester->WriteLogsForStripe(stripe);
        }
        else
        {
            break;
        }

        writtenLogs.push_back(stripe);

        if (writtenLogs.size() == 1)
        {
            // When previous log group written with dummy data is ready
            // to be checkpointed, trigger checkpoint
            ExpectCheckpointTriggered();
            journal->StartCheckpoint();
        }
    }

    writeTester->WaitForAllLogWriteDone();

    SimulateSPORWithoutRecovery();

    replayTester->ExpectReturningUnmapStripes();
    for (auto stripeLog : writtenLogs)
    {
        replayTester->ExpectReplayFullStripe(stripeLog);
    }

    replayTester->ExpectReplayFlushedActiveStripe();

    EXPECT_TRUE(journal->DoRecoveryForTest() == 0);
}

TEST_F(ReplayLogBufferIntegrationTest, ReplayFullLogBufferWhenSegmentContextFlushedAndVSAMapNotFlushedDuringCheckpoint)
{
    POS_TRACE_DEBUG(9999, "ReplayLogBufferIntegrationTest::ReplayFullLogBufferWhenSegmentContextFlushedAndVSAMapNotFlushedDuringCheckpoint");

    uint32_t numSegments = 1;
    uint32_t numStripes = testInfo->numStripesPerSegment * numSegments;
    uint64_t sizeLogGroupAlignByMpage = _CalculateLogBufferSize(numStripes / 2 , 8);
    JournalConfigurationBuilder builder(testInfo);
    builder.SetJournalEnable(true)
        ->SetLogBufferSize(sizeLogGroupAlignByMpage * 2);

    InitializeJournal(builder.Build());
    SetTriggerCheckpoint(false);
    uint32_t stripeIndex = 0;
    std::vector<StripeTestFixture> writtenStripesForLogGoup0;
    for (stripeIndex = 0; stripeIndex < numStripes / 2; stripeIndex++)
    {
        StripeTestFixture stripe(stripeIndex, testInfo->defaultTestVol);
        writeTester->GenerateLogsForStripe(stripe, 0, testInfo->numBlksPerStripe);
        writeTester->WriteLogsForStripe(stripe);
        writtenStripesForLogGoup0.push_back(stripe);
    }

    std::vector<StripeTestFixture> writtenStripesForLogGoup1BeforeCheckpoint;
    for (uint32_t index = 0; index < 10; index++, stripeIndex++)
    {
        StripeTestFixture stripe(stripeIndex, testInfo->defaultTestVol);
        writeTester->GenerateLogsForStripe(stripe, 0, testInfo->numBlksPerStripe);
        writeTester->WriteLogsForStripe(stripe);
        writtenStripesForLogGoup1BeforeCheckpoint.push_back(stripe);
    }
    std::vector<StripeTestFixture> writtenStripesForLogGoup1AfterCheckpoint;
    for (uint32_t index = 0; index < numStripes / 2 - 10; index++, stripeIndex++)
    {
        StripeTestFixture stripe(stripeIndex, testInfo->defaultTestVol);
        writeTester->GenerateLogsForStripe(stripe, 0, testInfo->numBlksPerStripe);
        writeTester->WriteLogsForStripe(stripe);
        writtenStripesForLogGoup1AfterCheckpoint.push_back(stripe);
    }
    writeTester->WaitForAllLogWriteDone();

    uint64_t latestContextVersion = 1;
    EXPECT_CALL(*(testAllocator->GetIContextManagerMock()), GetStoredContextVersion(SEGMENT_CTX))
        .WillOnce(Return(0))
        .WillRepeatedly(Return(1));

    EXPECT_CALL(*testMapper, FlushDirtyMpages).Times(1);
    ON_CALL(*testMapper, FlushDirtyMpages).WillByDefault([&](int volId, EventSmartPtr event) {
        ((LogGroupReleaserSpy*)(journal->GetLogGroupReleaser()))->ForceCompleteCheckpoint();
        return -1;
    });
    journal->StartCheckpoint();

    WaitForAllCheckpointDone();
    SimulateSPORWithoutRecovery(builder);

    int64_t actualValidCount = numStripes * testInfo->numBlksPerStripe / 2;
    ON_CALL(*(testAllocator->GetISegmentCtxMock()), ValidateBlks).WillByDefault([&](VirtualBlks blks) {
        actualValidCount++;
        return true;
    });
    ON_CALL(*(testAllocator->GetISegmentCtxMock()), InvalidateBlks).WillByDefault([&](VirtualBlks blks, bool isForced) {
        actualValidCount--;
        return true;
    });

    replayTester->ExpectReturningUnmapStripes();
    for (auto stripeLog : writtenStripesForLogGoup0)
    {
        replayTester->ExpectReplayFullStripeWithAlreadyUpdatedSegmentContext(stripeLog);
    }
    for (auto stripeLog : writtenStripesForLogGoup1BeforeCheckpoint)
    {
        replayTester->ExpectReplayFullStripe(stripeLog);
    }
    for (auto stripeLog : writtenStripesForLogGoup1AfterCheckpoint)
    {
        replayTester->ExpectReplayFullStripe(stripeLog);
    }

    /*
    ResetActiveStripeTail
    ReplaySsdLsid
    ReplayStripeFlushed
    ReplayStripeRelease
    */
    EXPECT_TRUE(journal->DoRecoveryForTest() == 0);
    int64_t expectedValidCount = numStripes * testInfo->numBlksPerStripe;
    EXPECT_EQ(expectedValidCount, actualValidCount);
}
} // namespace pos
