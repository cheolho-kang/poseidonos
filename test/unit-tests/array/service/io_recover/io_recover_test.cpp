#include "src/array/service/io_recover/io_recover.h"

#include <gtest/gtest.h>

#include "src/array/ft/raid1.h"
#include "src/include/pos_event_id.h"
#include "test/unit-tests/array/partition/stripe_partition_mock.h"

using ::testing::Return;
namespace pos
{
TEST(IORecover, IORecover_testConstructor)
{
    // Given

    // When
    IORecover ioRecover;

    // Then
}

TEST(IORecover, GetRecoverMethod_testWhenMethodIsEmpty)
{
    // Given
    IORecover ioRecover;
    ArrayRecover recover;
    unsigned int arrayIndex = 0;
    ioRecover.Register(arrayIndex, recover);

    // When
    RecoverMethod empty;
    int actual = ioRecover.GetRecoverMethod(arrayIndex, nullptr, empty);

    // Then
    int NO_METHOD = (int)POS_EVENT_ID::ARRAY_WRONG_NAME;
    ASSERT_EQ(NO_METHOD, actual);
}

TEST(IORecover, GetRecoverMethod_testWhenMethodIsSet)
{
    // Given
    IORecover ioRecover;
    ArrayRecover recover;
    string mockArrayName = "mockArray";
    unsigned int mockArrayIndex = 0;
    PartitionType mockPartitionType = PartitionType::USER_DATA;
    vector<ArrayDevice*> devs;
    PartitionPhysicalSize physicalSize{
        .startLba = 5,
        .blksPerChunk = 10,
        .chunksPerStripe = 4,
        .stripesPerSegment = 20,
        .totalSegments = 30};
    Raid1* method = new Raid1(&physicalSize);
    MockStripePartition* mockPart = new MockStripePartition(mockArrayName, mockArrayIndex, mockPartitionType, physicalSize, devs, method);
    recover.emplace(mockPartitionType, mockPart);
    unsigned int arrayIndex = 0;
    ioRecover.Register(arrayIndex, recover);
    int METHOD_RETURNED = 0;
    EXPECT_CALL(*mockPart, GetRecoverMethod).WillOnce(Return(METHOD_RETURNED));

    // When
    RecoverMethod out;
    int actual = ioRecover.GetRecoverMethod(arrayIndex, nullptr, out);

    // Then
    ASSERT_EQ(METHOD_RETURNED, actual);
}

TEST(IORecover, Register_testIfArgumentsAreValid)
{
    // Given
    IORecover ioRecover;
    ArrayRecover recover;
    string mockArrayName = "mockArray";
    unsigned int mockArrayIndex = 0;
    PartitionType mockPartitionType = PartitionType::USER_DATA;
    vector<ArrayDevice*> devs;
    PartitionPhysicalSize physicalSize{
        .startLba = 5,
        .blksPerChunk = 10,
        .chunksPerStripe = 4,
        .stripesPerSegment = 20,
        .totalSegments = 30};
    Raid1* method = new Raid1(&physicalSize);
    MockStripePartition* mockPart = new MockStripePartition(mockArrayName, mockArrayIndex, mockPartitionType, physicalSize, devs, method);
    recover.emplace(mockPartitionType, mockPart);

    // When
    unsigned int arrayIndex = 0;
    bool actual = ioRecover.Register(arrayIndex, recover);

    // Then
    ASSERT_TRUE(actual);
}

TEST(IORecover, Unregister_testIfArgumentsAreValid)
{
    // Given
    IORecover ioRecover;
    ArrayRecover recover;
    string mockArrayName = "mockArray";
    unsigned int mockArrayIndex = 0;
    PartitionType mockPartitionType = PartitionType::USER_DATA;
    vector<ArrayDevice*> devs;
    PartitionPhysicalSize physicalSize{
        .startLba = 5,
        .blksPerChunk = 10,
        .chunksPerStripe = 4,
        .stripesPerSegment = 20,
        .totalSegments = 30};
    Raid1* method = new Raid1(&physicalSize);
    MockStripePartition* mockPart = new MockStripePartition(mockArrayName, mockArrayIndex, mockPartitionType, physicalSize, devs, method);
    recover.emplace(mockPartitionType, mockPart);
    unsigned int arrayIndex = 0;
    ioRecover.Register(arrayIndex, recover);

    // When
    ioRecover.Unregister(arrayIndex);

    // Then
}

} // namespace pos