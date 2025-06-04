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

#ifndef SERVICES_BIDDING_SERVICE_ROUTING_UTILS_H_
#define SERVICES_BIDDING_SERVICE_ROUTING_UTILS_H_

#include <future>  // Include necessary header for async and future
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/microsoft_routing_utils.grpc.pb.h"
#include "api/microsoft_routing_utils.pb.h"
#include "services/bidding_service/data/runtime_config.h"
#include "services/bidding_service/microsoft_routing/connection_pool.h"
#include "services/common/clients/async_grpc/default_async_grpc_client.h"
#include "services/common/clients/client_params.h"
#include "services/common/clients/code_dispatcher/request_context.h"
#include "services/common/clients/config/trusted_server_config_client.h"
#include "services/common/clients/http/multi_curl_http_fetcher_async.h"
#include "services/common/clients/kv_server/kv_async_client.h"
#include "services/common/util/client_contexts.h"
#include "src/roma/interface/roma.h"

using privacy_sandbox::bidding_auction_servers::BiddingServiceRuntimeConfig;
using privacy_sandbox::bidding_auction_servers::ClientContexts;
using privacy_sandbox::bidding_auction_servers::CryptoClientWrapperInterface;
using privacy_sandbox::bidding_auction_servers::KVAsyncGrpcClient;
using privacy_sandbox::bidding_auction_servers::RequestMetadata;
using privacy_sandbox::bidding_auction_servers::RomaRequestSharedContext;
using privacy_sandbox::bidding_auction_servers::TrustedServersConfigClient;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;

namespace microsoft::routing_utils {

// Define the routing function name for Roma Service
constexpr absl::string_view kRoutingFunctionName = "fetchAdditionalSignals";

// The RoutingUtils class provides utility functions for routing requests from
// "generateBid" JavaScript function to the external services. Currently, it
// supports sending requests to key-value servers.
class RoutingUtils {
 public:
  // |config_client| A reference to the TrustedServersConfigClient
  // |key_fetcher_manager| A pointer to the KeyFetcherManagerInterface
  RoutingUtils(const TrustedServersConfigClient& config_client,
               KeyFetcherManagerInterface* key_fetcher_manager);

  virtual ~RoutingUtils() = default;

  // Fetches additional signals for UDF function.
  // |wrapper| A reference to the FunctionBindingPayload with request context.
  void FetchAdditionalSignals(
      google::scp::roma::FunctionBindingPayload<RomaRequestSharedContext>&
          wrapper);

 protected:
  virtual void SendRequestToKVServer(
      const proto::KVGetValuesRequest& request,
      proto::BatchKVGetValuesResponse& batch_response,
      std::atomic<int>& batch_response_counter);

 private:
  // Keeps track of the client contexts used for RPC calls
  ClientContexts client_contexts_;

  std::unique_ptr<ConnectionPool> connection_pool_;
  int selection_kv_server_timeout_ms_;
};

}  // namespace microsoft::routing_utils

#endif  // SERVICES_BIDDING_SERVICE_ROUTING_UTILS_H_
