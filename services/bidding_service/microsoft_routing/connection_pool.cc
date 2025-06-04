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

#include "services/bidding_service/microsoft_routing/connection_pool.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/server_posix.h>

#include "absl/base/const_init.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "services/common/clients/code_dispatcher/request_context.h"
#include "services/common/util/request_response_constants.h"
#include "src/logger/request_context_logger.h"
#include "src/roma/interface/roma.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

namespace microsoft::routing_utils {

ConnectionPool::ConnectionPool(
    const TrustedServersConfigClient& config_client,
    KeyFetcherManagerInterface* key_fetcher_manager) {
  PS_LOG(INFO) << "Init Connection Pool for Routing Utils";

  // Create connections to key-value servers
  auto tee_kv_server_addr = std::string(config_client.GetStringParameter(
      privacy_sandbox::bidding_auction_servers::TEE_KV_SERVER_ADDR));
  if (!tee_kv_server_addr.empty()) {
    CreateConnection(
        tee_kv_server_addr, kKeyValue,
        config_client.GetBooleanParameter(SELECTION_KV_SERVER_EGRESS_TLS),
        key_fetcher_manager);
  } else {
    PS_LOG(INFO)
        << "Key-Value Service address is empty. Connection is not created.";
  }

  // Create connections to ad retrieval key-value servers
  auto tee_ad_retrieval_kv_server_addr =
      std::string(config_client.GetStringParameter(
          privacy_sandbox::bidding_auction_servers::
              TEE_AD_RETRIEVAL_KV_SERVER_ADDR));
  if (!tee_ad_retrieval_kv_server_addr.empty()) {
    CreateConnection(tee_ad_retrieval_kv_server_addr, kAdRetrieval,
                     config_client.GetBooleanParameter(
                         privacy_sandbox::bidding_auction_servers::
                             AD_RETRIEVAL_KV_SERVER_EGRESS_TLS),
                     key_fetcher_manager);
  } else {
    PS_LOG(INFO) << "AD Retrieval Key-Value Service address is empty. "
                    "Connection is not created.";
  }

  // Create connections to selection key-value servers
  auto selection_kv_server_addr =
      std::string(config_client.GetStringParameter(SELECTION_KV_SERVER_ADDR));
  if (!selection_kv_server_addr.empty()) {
    CreateConnection(
        selection_kv_server_addr, kSelection,
        config_client.GetBooleanParameter(SELECTION_KV_SERVER_EGRESS_TLS),
        key_fetcher_manager);
  } else {
    PS_LOG(INFO) << "Selection Key-Value Service address is empty. "
                    "Connection is not created.";
  }

  PS_LOG(INFO) << "Connection Pool for Routing Utils initialized with "
               << async_clients_.size() << " connections";
}

void ConnectionPool::CreateConnection(
    const std::string& server_addr, absl::string_view server_name,
    bool is_connection_secure,
    KeyFetcherManagerInterface* key_fetcher_manager) {
  PS_LOG(INFO) << "Create connection to " << server_name
               << " address: " << server_addr
               << " secure: " << is_connection_secure;

  auto channel = privacy_sandbox::bidding_auction_servers::CreateChannel(
      server_addr,
      /*compression=*/true,
      /*secure=*/is_connection_secure);

  auto client_stub = kv_server::v2::KeyValueService::NewStub(channel);

  async_clients_[std::string(server_name)] =
      std::make_unique<KVAsyncGrpcClient>(key_fetcher_manager,
                                          std::move(client_stub));

  PS_LOG(INFO) << "Connection to " << server_name << " created";
}

absl::StatusOr<KVAsyncGrpcClient*> ConnectionPool::GetAsyncClient(
    const proto::KVServerInfo& server_info) {
  PS_LOG(INFO) << "Get async client for server: " << server_info.server_name();

  if (auto record = async_clients_.find(server_info.server_name());
      record != async_clients_.end())
    return record->second.get();
  else
    return absl::NotFoundError(
        "KVAsyncGrpcClient not found for the given server info.");
}

}  // namespace microsoft::routing_utils
