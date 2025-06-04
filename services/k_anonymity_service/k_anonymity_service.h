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

#ifndef SERVICES_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_H_
#define SERVICES_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_H_

#include <memory>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "api/microsoft_k_anonymity.grpc.pb.h"
#include "services/common/clients/async_grpc/default_async_grpc_client.h"
#include "services/k_anonymity_service/join_reactor.h"
#include "services/k_anonymity_service/query_reactor.h"
#include "src/concurrent/event_engine_executor.h"
#include "src/core/lib/event_engine/default_event_engine.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace microsoft::k_anonymity {

using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using proto::JoinRequest;
using proto::QueryRequest;
using proto::QueryResponse;

using QueryReactorFactory = absl::AnyInvocable<QueryReactor*(
    grpc::CallbackServerContext* context, const QueryRequest* request,
    QueryResponse* response, KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client,
    const KAnonymityServiceRuntimeConfig& runtime_config)>;

using JoinReactorFactory = absl::AnyInvocable<JoinReactor*(
    grpc::CallbackServerContext* context, const JoinRequest* request,
    google::protobuf::Empty* response,
    KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client,
    const KAnonymityServiceRuntimeConfig& runtime_config)>;

// KAnonymityService implements business logic for querying and joining k-anon
// data sets. It takes input from the BuyerFrontEndService and keeps track of if
// a given data set meets the definition of k-anonymous.
class KAnonymityService final : public proto::KAnonymity::CallbackService {
 public:
  explicit KAnonymityService(
      QueryReactorFactory query_reactor_factory,
      JoinReactorFactory join_reactor_factory,
      std::unique_ptr<KeyFetcherManagerInterface> key_fetcher_manager,
      std::unique_ptr<CryptoClientWrapperInterface> crypto_client,
      KAnonymityServiceRuntimeConfig runtime_config)
      : query_reactor_factory_(std::move(query_reactor_factory)),
        join_reactor_factory_(std::move(join_reactor_factory)),
        key_fetcher_manager_(std::move(key_fetcher_manager)),
        crypto_client_(std::move(crypto_client)),
        runtime_config_(std::move(runtime_config)) {}

  // queries and maintains k-anon data for a Buyer in an ad auction
  // orchestrated by the SellerFrontEndService.
  //
  // This is the API that is accessed by the BuyerFrontEndService. This is the
  // async RPC endpoint that will query or join k-anonymity of given sets.
  //
  // context: Standard gRPC-owned testing utility parameter.
  // request: Pointer to QueryRequest. The request is encrypted using
  // Hybrid Public Key Encryption (HPKE).
  // response: Pointer to QueryResponse. The response is encrypted
  // using HPKE.
  // return: a ServerUnaryReactor that is used by the gRPC library
  grpc::ServerUnaryReactor* Query(grpc::CallbackServerContext* context,
                                  const QueryRequest* request,
                                  QueryResponse* response) override;

  // Allows a distinct member to join a given data set.
  grpc::ServerUnaryReactor* Join(grpc::CallbackServerContext* context,
                                 const JoinRequest* request,
                                 google::protobuf::Empty* response) override;

 private:
  QueryReactorFactory query_reactor_factory_;
  JoinReactorFactory join_reactor_factory_;

  std::unique_ptr<KeyFetcherManagerInterface> key_fetcher_manager_;
  std::unique_ptr<CryptoClientWrapperInterface> crypto_client_;
  KAnonymityServiceRuntimeConfig runtime_config_;
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_H_
