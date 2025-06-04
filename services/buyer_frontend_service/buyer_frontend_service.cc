// Copyright 2022 Google LLC
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

#include "services/buyer_frontend_service/buyer_frontend_service.h"

#include <memory>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "api/bidding_auction_servers.pb.h"
#include "services/buyer_frontend_service/get_bids_unary_reactor.h"
#include "services/common/metric/server_definition.h"
#include "src/telemetry/telemetry.h"

#if defined(MICROSOFT_AD_SELECTION_BUILD)
#include "services/buyer_frontend_service/microsoft_join_k_anon_reactor.h"
#endif

namespace privacy_sandbox::bidding_auction_servers {

BuyerFrontEndService::BuyerFrontEndService(
    std::unique_ptr<BiddingSignalsAsyncProvider> bidding_signals_async_provider,
    const BiddingServiceClientConfig& client_config,
    std::unique_ptr<server_common::KeyFetcherManagerInterface>
        key_fetcher_manager,
    std::unique_ptr<CryptoClientWrapperInterface> crypto_client,
    std::unique_ptr<KVAsyncClient> kv_async_client, const GetBidsConfig config,
    server_common::Executor& executor, bool enable_benchmarking)
    : bidding_signals_async_provider_(
          config.bidding_signals_fetch_mode ==
                  BiddingSignalsFetchMode::NOT_FETCHED
              ? nullptr
              : std::move(bidding_signals_async_provider)),
      config_(config),
      enable_benchmarking_(enable_benchmarking),
      key_fetcher_manager_(std::move(key_fetcher_manager)),
      crypto_client_(std::move(crypto_client)),
      stub_(Bidding::NewStub(CreateChannel(
          client_config.server_addr, client_config.compression,
          client_config.secure_client, client_config.grpc_arg_default_authority,
          client_config.ca_root_pem))),
      bidding_async_client_(std::make_unique<BiddingAsyncGrpcClient>(
          key_fetcher_manager_.get(), crypto_client_.get(), client_config,
          stub_.get())),
      kv_async_client_(config.bidding_signals_fetch_mode ==
                               BiddingSignalsFetchMode::NOT_FETCHED
                           ? nullptr
                           : std::move(kv_async_client)),
      executor_(executor) {
  if (config_.is_protected_app_signals_enabled) {
    protected_app_signals_bidding_async_client_ =
        std::make_unique<ProtectedAppSignalsBiddingAsyncGrpcClient>(
            key_fetcher_manager_.get(), crypto_client_.get(), client_config,
            stub_.get());
  }

#if defined(MICROSOFT_AD_SELECTION_BUILD)
  // Initially a K-Anon instance is recommended but not required for testing as
  // this is not enforced by the clients. We will fail closed and stat all bids
  // are NOT k-anon without this though.
  if (!client_config.microsoft_k_anon_server_addr.empty()) {
    // Create our K-Anon gRPC stub and async clients for usage later on.
    microsoft_k_anon_stub_ = microsoft::k_anonymity::proto::KAnonymity::NewStub(
        CreateChannel(client_config.microsoft_k_anon_server_addr,
                      client_config.compression, client_config.secure_client,
                      client_config.grpc_arg_default_authority));
    microsoft_kanon_query_async_client_ =
        std::make_unique<microsoft::k_anonymity::KAnonQueryAsyncGrpcClient>(
            key_fetcher_manager_.get(), crypto_client_.get(),
            microsoft_k_anon_stub_.get());
    microsoft_kanon_join_async_client_ =
        std::make_unique<microsoft::k_anonymity::KAnonJoinAsyncGrpcClient>(
            key_fetcher_manager_.get(), crypto_client_.get(),
            microsoft_k_anon_stub_.get());
  }
#endif
}

BuyerFrontEndService::BuyerFrontEndService(ClientRegistry client_registry,
                                           const GetBidsConfig config,
                                           server_common::Executor& executor,
                                           bool enable_benchmarking)
    : bidding_signals_async_provider_(
          config.bidding_signals_fetch_mode ==
                  BiddingSignalsFetchMode::NOT_FETCHED
              ? nullptr
              : std::move(client_registry.bidding_signals_async_provider)),
      config_(config),
      enable_benchmarking_(enable_benchmarking),
      key_fetcher_manager_(std::move(client_registry.key_fetcher_manager)),
      crypto_client_(std::move(client_registry.crypto_client)),
      bidding_async_client_(std::move(client_registry.bidding_async_client)),
      protected_app_signals_bidding_async_client_(std::move(
          client_registry.protected_app_signals_bidding_async_client)),
      kv_async_client_(config.bidding_signals_fetch_mode ==
                               BiddingSignalsFetchMode::NOT_FETCHED
                           ? nullptr
                           : std::move(client_registry.kv_async_client)),
      executor_(executor) {
#if defined(MICROSOFT_AD_SELECTION_BUILD)
  microsoft_kanon_query_async_client_ =
      std::move(client_registry.microsoft_kanon_query_async_client);
  microsoft_kanon_join_async_client_ =
      std::move(client_registry.microsoft_kanon_join_async_client);
#endif
}

grpc::ServerUnaryReactor* BuyerFrontEndService::GetBids(
    grpc::CallbackServerContext* context, const GetBidsRequest* request,
    GetBidsResponse* response) {
  LogCommonMetric(request, response);

  // Will be deleted in onDone
  auto reactor = std::make_unique<GetBidsUnaryReactor>(
      *context, *request, *response, bidding_signals_async_provider_.get(),
#if defined(MICROSOFT_AD_SELECTION_BUILD)
      *microsoft_kanon_query_async_client_,
#endif
      *bidding_async_client_, config_,
      protected_app_signals_bidding_async_client_.get(),
      key_fetcher_manager_.get(), crypto_client_.get(), kv_async_client_.get(),
      executor_, enable_benchmarking_);
  reactor->Execute();
  return reactor.release();
}

#if defined(MICROSOFT_AD_SELECTION_BUILD)
grpc::ServerUnaryReactor* BuyerFrontEndService::MicrosoftJoinKAnon(
    grpc::CallbackServerContext* context,
    const MicrosoftJoinKAnonRequest* request,
    google::protobuf::Empty* response) {
  LogCommonMetric(request, response);

  // Will be deleted in onDone
  auto reactor = std::make_unique<MicrosoftJoinKAnonReactor>(
      *context, *request, *response, *microsoft_kanon_join_async_client_,
      config_, key_fetcher_manager_.get(), crypto_client_.get(),
      enable_benchmarking_);
  reactor->Execute();
  return reactor.release();
}
#endif
}  // namespace privacy_sandbox::bidding_auction_servers
