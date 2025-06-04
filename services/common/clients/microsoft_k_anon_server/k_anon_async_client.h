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

#ifndef SERVICES_COMMON_CLIENTS_K_ANON_ASYNC_CLIENT_H_
#define SERVICES_COMMON_CLIENTS_K_ANON_ASYNC_CLIENT_H_

#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "api/bidding_auction_servers.grpc.pb.h"
#include "api/microsoft_k_anonymity.grpc.pb.h"
#include "api/microsoft_k_anonymity.pb.h"
#include "services/common/clients/async_grpc/default_async_grpc_client.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::AsyncClient;
using privacy_sandbox::bidding_auction_servers::CryptoClientWrapperInterface;
using privacy_sandbox::bidding_auction_servers::DefaultAsyncGrpcClient;
using privacy_sandbox::bidding_auction_servers::GetBidsRequest;
using privacy_sandbox::bidding_auction_servers::GetBidsResponse;
using privacy_sandbox::bidding_auction_servers::RawClientParams;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using privacy_sandbox::server_common::LogContext;

using QueryAsyncClient = AsyncClient<proto::QueryRequest, proto::QueryResponse,
                                     proto::QueryRequest::QueryRawRequest,
                                     proto::QueryResponse::QueryRawResponse>;

using JoinAsyncClient =
    AsyncClient<proto::JoinRequest, google::protobuf::Empty,
                proto::JoinRequest::JoinRawRequest, google::protobuf::Empty>;

// Helpers to create/handle Query requests
std::unique_ptr<proto::QueryRequest::QueryRawRequest> CreateBidsQueryRawRequest(
    const GetBidsResponse::GetBidsRawResponse& get_bid_raw_response,
    const LogContext& log_context);

void AttachBiddingMetadata(
    absl::StatusOr<std::unique_ptr<proto::QueryResponse::QueryRawResponse>>
        query_raw_response,
    GetBidsResponse::GetBidsRawResponse& get_bid_raw_response);

std::string GetKAnonHash(const std::string& ad_creative_url);

// This class is an async grpc client for the Query function of the KAnonymity
// Service.
class KAnonQueryAsyncGrpcClient
    : public DefaultAsyncGrpcClient<proto::QueryRequest, proto::QueryResponse,
                                    proto::QueryRequest::QueryRawRequest,
                                    proto::QueryResponse::QueryRawResponse> {
 public:
  KAnonQueryAsyncGrpcClient(KeyFetcherManagerInterface* key_fetcher_manager,
                            CryptoClientWrapperInterface* crypto_client,
                            proto::KAnonymity::Stub* stub);

 protected:
  // Sends an asynchronous request via grpc to the KAnonymity Service.
  //
  // params: a pointer to the ClientParams object which carries data used
  // by the grpc stub.
  void SendRpc(const std::string& hpke_secret, grpc::ClientContext* context,
               RawClientParams<proto::QueryRequest, proto::QueryResponse,
                               proto::QueryResponse::QueryRawResponse>* params)
      const override;

  proto::KAnonymity::Stub* stub_;
};

class KAnonJoinAsyncGrpcClient
    : public DefaultAsyncGrpcClient<proto::JoinRequest, google::protobuf::Empty,
                                    proto::JoinRequest::JoinRawRequest,
                                    google::protobuf::Empty> {
 public:
  KAnonJoinAsyncGrpcClient(KeyFetcherManagerInterface* key_fetcher_manager,
                           CryptoClientWrapperInterface* crypto_client,
                           proto::KAnonymity::Stub* stub);

 protected:
  // Sends an asynchronous request via grpc to the Join function of the
  // KAnonymity Service.
  //
  // params: a pointer to the ClientParams object which carries data used
  // by the grpc stub.
  void SendRpc(const std::string& hpke_secret, grpc::ClientContext* context,
               RawClientParams<proto::JoinRequest, google::protobuf::Empty,
                               google::protobuf::Empty>* params) const override;

  proto::KAnonymity::Stub* stub_;
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_COMMON_CLIENTS_K_ANON_ASYNC_CLIENT_H_
