// Copyright (C) Microsoft Corporation. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SERVICES_K_ANONYMITY_SERVICE_JOIN_REACTOR_H_
#define SERVICES_K_ANONYMITY_SERVICE_JOIN_REACTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/status/statusor.h"
#include "api/bidding_auction_servers.pb.h"
#include "services/common/metric/server_definition.h"
#include "services/k_anonymity_service/base_kanon_reactor.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_benchmarking_logger.h"
#include "services/k_anonymity_service/data/runtime_config.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::metric::KAnonymityJoinContext;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using proto::JoinRequest;
using proto::QueryResponse;

//  This is a gRPC reactor that serves a single JoinRequest.
//  It stores state relevant to the request and after the
//  response is finished being served, JoinReactor cleans up all
//  necessary state and grpc releases the reactor from memory.
class JoinReactor
    : public BaseKAnonReactor<JoinRequest, JoinRequest::JoinRawRequest,
                              const google::protobuf::Empty,
                              QueryResponse::QueryRawResponse> {
 public:
  explicit JoinReactor(
      grpc::CallbackServerContext* context, const JoinRequest* request,
      const google::protobuf::Empty* response,
      std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarking_logger,
      KeyFetcherManagerInterface* key_fetcher_manager,
      CryptoClientWrapperInterface* crypto_client,
      const KAnonymityServiceRuntimeConfig& runtime_config);

  // Initiate the asynchronous execution of the JoinRequest.
  void Execute() override;

 private:
  // Cleans up and deletes the JoinReactor. Called by the grpc library
  // after the response has finished.
  void OnDone() override;

  // Encrypts the response before the GRPC call is finished with the provided
  // status.
  void EncryptResponseAndFinish(grpc::Status status);

  std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarking_logger_;

  // Used to log metric, same life time as reactor.
  std::unique_ptr<KAnonymityJoinContext> metric_context_;
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_K_ANONYMITY_SERVICE_JOIN_REACTOR_H_
