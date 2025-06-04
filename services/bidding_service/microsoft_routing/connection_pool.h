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

#ifndef SERVICES_BIDDING_SERVICE_ROUTING_UTILS_CONNECTION_POOL_H_
#define SERVICES_BIDDING_SERVICE_ROUTING_UTILS_CONNECTION_POOL_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "api/microsoft_routing_utils.grpc.pb.h"
#include "api/microsoft_routing_utils.pb.h"
#include "services/bidding_service/data/runtime_config.h"
#include "services/bidding_service/microsoft_routing/routing_flags.h"
#include "services/bidding_service/runtime_flags.h"
#include "services/common/clients/async_grpc/default_async_grpc_client.h"
#include "services/common/clients/async_grpc/grpc_client_utils.h"
#include "services/common/clients/code_dispatcher/request_context.h"
#include "services/common/clients/config/trusted_server_config_client.h"
#include "services/common/clients/http/multi_curl_http_fetcher_async.h"
#include "services/common/clients/kv_server/kv_async_client.h"

using privacy_sandbox::bidding_auction_servers::BiddingServiceRuntimeConfig;
using privacy_sandbox::bidding_auction_servers::KVAsyncGrpcClient;
using privacy_sandbox::bidding_auction_servers::TrustedServersConfigClient;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;

namespace microsoft::routing_utils {

constexpr absl::string_view kAdRetrieval = "AD_RETRIEVAL_KV_SERVER";
constexpr absl::string_view kKeyValue = "KV_SERVER";
constexpr absl::string_view kSelection = "SELECTION_KV_SERVER";

// Manages a pool of asynchronous gRPC clients for key-value servers.
// The ConnectionPool class is responsible for maintaining and providing access
// to a collection of asynchronous gRPC clients that communicate with key-value
// servers. It ensures that connections are created and managed efficiently,
// allowing for secure and reliable communication with the servers.
// For each key-value server, the ConnectionPool class creates one separate
// gRPC client and reuse it for all the requests to that server.
class ConnectionPool {
 public:
  // |config_client| A reference to the TrustedServersConfigClient
  // |key_fetcher_manager| A pointer to the KeyFetcherManagerInterface
  ConnectionPool(const TrustedServersConfigClient& config_client,
                 KeyFetcherManagerInterface* key_fetcher_manager);

  // Retrieves an asynchronous gRPC client for the specified server.
  // |server_info| Information about the key-value server. The server name will
  // be used to identify the client.
  absl::StatusOr<KVAsyncGrpcClient*> GetAsyncClient(
      const proto::KVServerInfo& server_info);

 private:
  // Async gRPC clients to the key-value servers
  std::map<std::string, std::unique_ptr<KVAsyncGrpcClient>> async_clients_;

  void CreateConnection(const std::string& server_addr,
                        absl::string_view server_name,
                        bool is_connection_secure,
                        KeyFetcherManagerInterface* key_fetcher_manager);
};

}  // namespace microsoft::routing_utils

#endif  // SERVICES_BIDDING_SERVICE_ROUTING_UTILS_CONNECTION_POOL_H_
