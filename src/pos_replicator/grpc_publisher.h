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

#pragma once

#include <grpc++/grpc++.h>
#include <memory>
#include <string>

#include "proto/generated/cpp/replicator_rpc.grpc.pb.h"
#include "proto/generated/cpp/replicator_rpc.pb.h"
#include "src/helper/json/json_helper.h"

namespace pos
{
class ConfigManager;

class GrpcPublisher
{
public:
    GrpcPublisher(std::shared_ptr<grpc::Channel> channel_, ConfigManager* configManager);
    ~GrpcPublisher(void);

    int PushHostWrite(uint64_t rba, uint64_t size, string volumeName, string arrayName, void* buffer, uint64_t& lsn);
    int CompleteUserWrite(uint64_t lsn, string volumeName, string arrayName);
    int CompleteWrite(uint64_t lsn, string volumeName, string arrayName);
    int CompleteRead(string arrayName, string volumeName, uint64_t rba, uint64_t numBlocks, uint64_t lsn, void* buffer);

private:
    void _ConnectGrpcServer(std::string targetAddress);
    bool _WaitUntilReady(void);
    void _InsertBlockToChunk(replicator_rpc::CompleteReadRequest* request, void* data, uint64_t numBlocks);

    ConfigManager* configManager;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<replicator_rpc::ReplicatorIoService::Stub> stub;
};
} // namespace pos
