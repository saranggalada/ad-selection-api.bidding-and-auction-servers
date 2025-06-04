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
#include "services/bidding_service/microsoft_routing/routing_utils.h"

#include <atomic>
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

RoutingUtils::RoutingUtils(const TrustedServersConfigClient& config_client,
                           KeyFetcherManagerInterface* key_fetcher_manager) {
  connection_pool_ =
      std::make_unique<ConnectionPool>(config_client, key_fetcher_manager);

  selection_kv_server_timeout_ms_ =
      config_client.GetIntParameter(SELECTION_KV_SERVER_TIMEOUT_MS);

  PS_LOG(INFO) << "Routing Utils Initialized";
}

void RoutingUtils::FetchAdditionalSignals(
    google::scp::roma::FunctionBindingPayload<RomaRequestSharedContext>&
        wrapper) {
  const std::string& payload = wrapper.io_proto.input_string();
  proto::BatchKVGetValuesRequest batch_request;

  // Convert JSON string to Protobuf message
  auto status =
      google::protobuf::util::JsonStringToMessage(payload, &batch_request);
  if (!status.ok()) {
    PS_LOG(ERROR) << "Failed to parse JSON to Protobuf from Payload: "
                  << status.ToString();
    return;
  }
  PS_LOG(INFO) << "Successfully Parsed JSON to Protobuf. Batch Request: "
               << batch_request.DebugString();

  if (batch_request.requests_size() == 0) {
    PS_LOG(INFO) << "Batch Request is empty. Nothing to process.";
    return;
  }

  proto::BatchKVGetValuesResponse batch_response;
  std::atomic<int> batch_response_counter(0);
  for (const auto& kv_request : batch_request.requests()) {
    // Send Async requests to KV Server
    SendRequestToKVServer(kv_request, batch_response, batch_response_counter);
  }

  // Wait for all requests to be processed.
  while (batch_response_counter.load() < batch_request.requests_size()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  PS_LOG(INFO)
      << "All requests to KV Server are processed. Sending response to Roma: \n"
      << batch_response.DebugString();

  // Convert the message to JSON
  std::string json_string;
  auto conv_status =
      google::protobuf::util::MessageToJsonString(batch_response, &json_string);
  if (conv_status.ok()) {
    wrapper.io_proto.set_output_string(json_string);
  } else {
    wrapper.io_proto.set_output_string(status.message());
  }
}

void RoutingUtils::SendRequestToKVServer(
    const proto::KVGetValuesRequest& request,
    proto::BatchKVGetValuesResponse& batch_response,
    std::atomic<int>& batch_response_counter) {
  PS_LOG(INFO) << "Send Request to KV Server: "
               << request.server_info().server_name();

  auto kv_async_client =
      connection_pool_->GetAsyncClient(request.server_info());

  if (!kv_async_client.ok() || (*kv_async_client == nullptr)) {
    PS_LOG(ERROR) << "Failed to get async client for KV server: "
                  << request.server_info().server_name();

    auto* kv_response = batch_response.add_responses();
    kv_response->set_server_name(request.server_info().server_name());
    kv_response->set_error_message("Failed to get async client for KV server");
    batch_response_counter.fetch_add(1);

    return;
  }

  grpc::ClientContext* client_context = client_contexts_.Add();

  auto status =
      (*kv_async_client)
          ->ExecuteInternal(
              std::make_unique<
                  privacy_sandbox::bidding_auction_servers::GetValuesRequest>(
                  request.get_values_request()),
              client_context,
              [&](absl::StatusOr<
                      std::unique_ptr<privacy_sandbox::bidding_auction_servers::
                                          GetValuesResponse>>
                      response,
                  privacy_sandbox::bidding_auction_servers::ResponseMetadata
                      metadata) {
                auto* kv_response = batch_response.add_responses();
                kv_response->set_server_name(
                    request.server_info().server_name());

                if (response.ok()) {
                  kv_response->set_allocated_get_values_response(
                      response->release());

                } else {
                  kv_response->set_error_message(response.status().message());
                }

                batch_response_counter.fetch_add(1);
              },
              /*timeout=*/absl::Milliseconds(selection_kv_server_timeout_ms_));

  if (!status.ok()) {
    PS_LOG(INFO) << "Failed to send request to KV server: "
                 << request.server_info().server_name()
                 << " Status: " << status;

    auto* kv_response = batch_response.add_responses();
    kv_response->set_server_name(request.server_info().server_name());
    kv_response->set_error_message(status.message());
    batch_response_counter.fetch_add(1);
  }
}

}  // namespace microsoft::routing_utils
