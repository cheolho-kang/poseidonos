/*
 *   BSD LICENSE
 *   Copyright (c) 2021 Samsung Electronics Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Corporation nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/metafs/mim/mpio.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <list>
#include <memory>

#include "test/unit-tests/metafs/mim/mdpage_mock.h"
#include "test/unit-tests/metafs/storage/mss_mock.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace pos
{
class MpioTester : public Mpio
{
public:
    MpioTester(NiceMock<MockMDPage>* mdPage)
    : Mpio(mdPage, false, false, nullptr)
    {
        mss = new NiceMock<MockMetaStorageSubsystem>(0);
    }

    MpioTester(void* mdPageBuf)
    : Mpio(mdPageBuf, false, false)
    {
        mss = new NiceMock<MockMetaStorageSubsystem>(0);
    }
    virtual ~MpioTester(void)
    {
        delete mss;
    }
    MpioType GetType(void) const override
    {
        return MpioType::Read;
    }
    void HandleAsyncMemOpDone(void)
    {
        Mpio::_HandleAsyncMemOpDone(this);
    }
    void CallbackTest(Mpio* mpio)
    {
        ((MpioTester*)mpio)->cbTestResult = true;
    }
    void Setup(MpioIoInfo& mpioIoInfo, bool partialIO, bool forceSyncIO)
    {
        EXPECT_CALL(*mss, IsAIOSupport);
        Mpio::Setup(mpioIoInfo, partialIO, forceSyncIO, mss);
    }

    bool cbTestResult = false;

private:
    void _InitStateHandler(void) override
    {
    }

    NiceMock<MockMetaStorageSubsystem>* mss;
};

TEST(MpioTester, Mpio_testConstructor)
{
    MpioIoInfo ioInfo;
    ioInfo.fileType = MetaFileType::Journal;
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);

    MpioTester mpio(buf);
    mpio.Setup(ioInfo, true, true);

    EXPECT_EQ(mpio.GetCurrState(), MpAioState::Init);
    EXPECT_EQ(mpio.GetFileType(), ioInfo.fileType);

    free(buf);
}

TEST(MpioTester, Mpio_testCallbackForMemcpy)
{
    MpioIoInfo ioInfo;
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);

    MpioTester mpio(buf);
    mpio.Setup(ioInfo, true, true);

    PartialMpioDoneCb notifier = AsEntryPointParam1(&MpioTester::CallbackTest, &mpio);
    mpio.SetPartialDoneNotifier(notifier);
    mpio.HandleAsyncMemOpDone();

    EXPECT_EQ(mpio.cbTestResult, true);

    free(buf);
}

TEST(MpioTester, Id_testIfMpiosCanHaveThereUniqueIdThatIncreasesOneMpioAfterAnother)
{
    const uint64_t COUNT = 100;
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    std::list<std::shared_ptr<Mpio>> mpioList;

    std::shared_ptr<Mpio> ptr = std::make_shared<MpioTester>(buf);
    uint64_t firstId = ptr->GetId();

    for (uint64_t i = 1; i <= COUNT; ++i)
    {
        mpioList.emplace_back(std::make_shared<MpioTester>(buf));
        EXPECT_EQ(mpioList.back()->GetId(), firstId + i);
    }

    EXPECT_EQ(mpioList.size(), COUNT);

    uint64_t index = 1;
    for (auto mpio : mpioList)
    {
        EXPECT_EQ(mpio->GetId(), firstId + index);
        index++;
    }

    mpioList.clear();
    free(buf);
}

TEST(MpioTester, IsCached_testIfTheResultReturnsByMpioCacheState)
{
    // given
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    MpioTester mpio(buf);

    // then
    EXPECT_FALSE(mpio.IsCached());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Read);

    // then
    EXPECT_TRUE(mpio.IsCached());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Merge);

    // then
    EXPECT_TRUE(mpio.IsCached());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Write);

    // then
    EXPECT_TRUE(mpio.IsCached());
}

TEST(MpioTester, IsMergeable_testIfTheResultReturnsByMpioCacheState)
{
    // given
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    MpioTester mpio(buf);

    // then
    EXPECT_FALSE(mpio.IsMergeable());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Read);

    // then
    EXPECT_FALSE(mpio.IsMergeable());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Merge);

    // then
    EXPECT_FALSE(mpio.IsMergeable());

    // when
    mpio.ChangeCacheStateTo(MpioCacheState::Write);

    // then
    EXPECT_TRUE(mpio.IsMergeable());
}

TEST(MpioTester, IsCacheableVolumeType_testIfTheResultReturnsByMetaStorageType)
{
    // given
    MpioIoInfo ioInfo;
    char* buf = (char*)malloc(MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    memset(buf, 0, MetaFsIoConfig::META_PAGE_SIZE_IN_BYTES);
    MpioTester mpio(buf);

    // when
    ioInfo.targetMediaType = MetaStorageType::NVRAM;
    mpio.Setup(ioInfo, true, true);

    // when
    EXPECT_TRUE(mpio.IsCacheableVolumeType());

    // when
    ioInfo.targetMediaType = MetaStorageType::JOURNAL_SSD;
    mpio.Setup(ioInfo, true, true);

    // when
    EXPECT_TRUE(mpio.IsCacheableVolumeType());

    // when
    ioInfo.targetMediaType = MetaStorageType::SSD;
    mpio.Setup(ioInfo, true, true);

    // when
    EXPECT_FALSE(mpio.IsCacheableVolumeType());
}

TEST(MpioTester, CheckReadStatus_testIfThereIsNoException)
{
    NiceMock<MockMDPage>* mdPage = new NiceMock<MockMDPage>(nullptr);
    MpioTester mpio(mdPage);
    EXPECT_TRUE(mpio.CheckReadStatus(MpAioState::Complete));
}

TEST(MpioTester, CheckWriteStatus_testIfThereIsNoException)
{
    NiceMock<MockMDPage>* mdPage = new NiceMock<MockMDPage>(nullptr);
    MpioTester mpio(mdPage);
    EXPECT_TRUE(mpio.CheckWriteStatus(MpAioState::Complete));
}

TEST(MpioTester, DoE2ECheck_testIfDataIntegrityIsGood)
{
    // given
    NiceMock<MockMDPage>* mdPage = new NiceMock<MockMDPage>(nullptr);
    MpioTester mpio(mdPage);
    EXPECT_CALL(*mdPage, AttachControlInfo).WillOnce(Return());

    // when
    EXPECT_CALL(*mdPage, IsValidSignature).WillOnce(Return(true));
    EXPECT_CALL(*mdPage, CheckDataIntegrity).WillOnce(Return(EID(SUCCESS)));
    EXPECT_TRUE(mpio.DoE2ECheck(MpAioState::Complete));

    // then
    EXPECT_EQ(mpio.GetNextState(), MpAioState::Complete);
}

TEST(MpioTester, DoE2ECheck_testIfSignatureIsInvalidWhenFirstRead)
{
    // given
    NiceMock<MockMDPage>* mdPage = new NiceMock<MockMDPage>(nullptr);
    MpioTester mpio(mdPage);
    EXPECT_CALL(*mdPage, AttachControlInfo).WillOnce(Return());

    // when
    EXPECT_CALL(*mdPage, IsValidSignature).WillOnce(Return(false));

    // then
    EXPECT_TRUE(mpio.DoE2ECheck(MpAioState::Complete));
}

TEST(MpioTester, DoE2ECheck_testIfThereIsSomeProblemWithDataIntigrity)
{
    // given
    NiceMock<MockMDPage>* mdPage = new NiceMock<MockMDPage>(nullptr);
    MpioTester mpio(mdPage);
    EXPECT_CALL(*mdPage, AttachControlInfo).WillOnce(Return());

    // when
    EXPECT_CALL(*mdPage, IsValidSignature).WillOnce(Return(true));
    EXPECT_CALL(*mdPage, CheckDataIntegrity).WillOnce(Return(EID(MFS_INVALID_META_LPN)));
    mpio.DoE2ECheck(MpAioState::Complete);

    // then
    EXPECT_EQ(mpio.GetNextState(), MpAioState::Error);
}
} // namespace pos
