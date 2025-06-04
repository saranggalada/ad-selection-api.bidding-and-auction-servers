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

#ifndef SERVICES_BUYER_FRONTEND_SERVICE_MICROSOFT_JOIN_K_ANON_REACTOR_H_
#define SERVICES_BUYER_FRONTEND_SERVICE_MICROSOFT_JOIN_K_ANON_REACTOR_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/blocking_counter.h"
#include "api/bidding_auction_servers.grpc.pb.h"
#include "api/bidding_auction_servers.pb.h"
#include "services/buyer_frontend_service/data/get_bids_config.h"
#include "services/common/clients/microsoft_k_anon_server/k_anon_async_client.h"
#include "services/common/encryption/crypto_client_wrapper_interface.h"
#include "services/common/feature_flags.h"
#include "services/common/loggers/benchmarking_logger.h"
#include "services/common/loggers/request_log_context.h"
#include "services/common/metric/server_definition.h"
#include "services/common/util/client_contexts.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace privacy_sandbox::bidding_auction_servers {

// This is a gRPC server reactor that serves a single GetBidsRequest.
// It stores state relevant to the request and after the
// response is finished being served, it cleans up all
// necessary state and grpc releases the reactor from memory.
class MicrosoftJoinKAnonReactor : public grpc::ServerUnaryReactor {
 public:
  explicit MicrosoftJoinKAnonReactor(
      grpc::CallbackServerContext& context,
      const MicrosoftJoinKAnonRequest& join_request,
      google::protobuf::Empty& response,
      microsoft::k_anonymity::JoinAsyncClient&
          microsoft_kanon_join_async_client,
      const GetBidsConfig& config,
      server_common::KeyFetcherManagerInterface* key_fetcher_manager,
      CryptoClientWrapperInterface* crypto_client,
      bool enable_benchmarking = false);

  // MicrosoftJoinKAnonReactor is neither copyable nor movable.
  MicrosoftJoinKAnonReactor(const MicrosoftJoinKAnonReactor&) = delete;
  MicrosoftJoinKAnonReactor& operator=(const MicrosoftJoinKAnonReactor&) =
      delete;

  // Initiate the asynchronous execution of the JoinRequest.
  void Execute();

  // Runs once the request has finished execution and deletes current instance.
  void OnDone() override;

 private:
  // Decrypts the request ciphertext in and returns whether decryption was
  // successful. If successful, the result is written into 'raw_request_'.
  grpc::Status DecryptRequest();

  // Encrypts `raw_response` and sets the result on the 'response_ciphertext'
  // field in the response. Returns ok status if encryption succeeded.
  absl::Status EncryptResponse();

  // Gets logging context (as a key/val pair) that can help debug/trace a
  // request through the BA services.
  absl::btree_map<std::string, std::string> GetLoggingContext();

  // Finishes the RPC call with a status.
  void FinishWithStatus(const grpc::Status& status);

  // References for state, request, response and context from gRPC.
  // Should be released by gRPC
  // https://github.com/grpc/grpc/blob/dbc45208e2bfe14f01b1cbb06d0cd7c01077debb/include/grpcpp/server_context.h#L604
  grpc::CallbackServerContext* context_;
  const MicrosoftJoinKAnonRequest* request_;
  MicrosoftJoinKAnonRequest::MicrosoftJoinKAnonRawRequest raw_request_;

  const GetBidsConfig& config_;
  server_common::KeyFetcherManagerInterface* key_fetcher_manager_;
  CryptoClientWrapperInterface* crypto_client_;
  std::unique_ptr<BenchmarkingLogger> benchmarking_logger_;
  std::string hpke_secret_;

  // Clients to signal the k-anonymity service run by the buyer for Join
  // operations.
  microsoft::k_anonymity::JoinAsyncClient* microsoft_kanon_join_async_client_;
  // Metadata to be sent to bidding service.
  RequestMetadata microsoft_k_anon_metadata_;

  grpc::Status decrypt_status_;

  RequestLogContext log_context_;

  // Used to log metric, same life time as reactor.
  std::unique_ptr<metric::BfeJoinContext> metric_context_;

  // Keeps track of the client contexts used for RPC calls
  ClientContexts client_contexts_;

  // Logs GetBidsRawRequest if the consented debugging is enabled.
  void MayLogRawRequest();

  // Log metrics for the Initiated requests errors that were initiated by the
  // server
  void LogInitiatedRequestErrorMetrics(absl::string_view server_name,
                                       const absl::Status& status);
};

}  // namespace privacy_sandbox::bidding_auction_servers

#endif  // SERVICES_BUYER_FRONTEND_SERVICE_MICROSOFT_JOIN_K_ANON_REACTOR_H_
