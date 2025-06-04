//  Copyright (C) Microsoft Corporation. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "services/buyer_frontend_service/microsoft_join_k_anon_reactor.h"

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "api/bidding_auction_servers.grpc.pb.h"
#include "services/buyer_frontend_service/util/proto_factory.h"
#include "services/common/constants/user_error_strings.h"
#include "services/common/loggers/build_input_process_response_benchmarking_logger.h"
#include "services/common/loggers/no_ops_logger.h"
#include "services/common/util/request_metadata.h"
#include "services/common/util/request_response_constants.h"

namespace privacy_sandbox::bidding_auction_servers {

namespace {

using ::google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;

}  // namespace

MicrosoftJoinKAnonReactor::MicrosoftJoinKAnonReactor(
    grpc::CallbackServerContext& context,
    const MicrosoftJoinKAnonRequest& join_request,
    google::protobuf::Empty& response,
    microsoft::k_anonymity::JoinAsyncClient& microsoft_kanon_join_async_client,
    const GetBidsConfig& config,
    server_common::KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client, bool enable_benchmarking)
    : context_(&context),
      request_(&join_request),
      config_(config),
      key_fetcher_manager_(key_fetcher_manager),
      crypto_client_(crypto_client),
      microsoft_kanon_join_async_client_(&microsoft_kanon_join_async_client),
      log_context_([this]() {
        decrypt_status_ = DecryptRequest();
        return RequestLogContext(GetLoggingContext(),
                                 raw_request_.consented_debug_config());
      }()) {
  if (enable_benchmarking) {
    std::string request_id = FormatTime(absl::Now());
    benchmarking_logger_ =
        std::make_unique<BuildInputProcessResponseBenchmarkingLogger>(
            request_id);
  } else {
    benchmarking_logger_ = std::make_unique<NoOpsLogger>();
  }
  CHECK_OK([this]() {
    PS_ASSIGN_OR_RETURN(metric_context_,
                        metric::BfeJoinContextMap()->Remove(request_));
    if (log_context_.is_consented()) {
      metric_context_->SetConsented(raw_request_.log_context().generation_id());
    }
    return absl::OkStatus();
  }()) << "BfeJoinContextMap()->Get(request) should have been called";
}

grpc::Status MicrosoftJoinKAnonReactor::DecryptRequest() {
  if (request_->key_id().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, kEmptyKeyIdError};
  }

  if (request_->request_ciphertext().empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, kEmptyCiphertextError};
  }

  std::optional<server_common::PrivateKey> private_key =
      key_fetcher_manager_->GetPrivateKey(request_->key_id());
  if (!private_key.has_value()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, kInvalidKeyIdError};
  }

  absl::StatusOr<HpkeDecryptResponse> decrypt_response =
      crypto_client_->HpkeDecrypt(*private_key, request_->request_ciphertext());
  if (!decrypt_response.ok()) {
    return {grpc::StatusCode::INVALID_ARGUMENT, kMalformedCiphertext};
  }

  hpke_secret_ = std::move(*decrypt_response->mutable_secret());

  if (!raw_request_.ParseFromString(decrypt_response->payload())) {
    // If not, try to parse the request as before.
    return {grpc::StatusCode::INVALID_ARGUMENT, kMalformedCiphertext};
  }

  return grpc::Status::OK;
}

void MicrosoftJoinKAnonReactor::Execute() {
  // benchmarking_logger_->BuildInputBegin();

  PS_VLOG(kEncrypted, log_context_) << "Encrypted KAnon JoinRequest:\n"
                                    << request_->ShortDebugString();

  if (!decrypt_status_.ok()) {
    PS_LOG(ERROR, log_context_) << "Decrypting the request failed:"
                                << server_common::ToAbslStatus(decrypt_status_);
    FinishWithStatus(decrypt_status_);
    return;
  }

  PS_VLOG(kPlain, log_context_) << "JoinRawRequest:\n"
                                << raw_request_.ShortDebugString();

  if (!microsoft_kanon_join_async_client_) {
    PS_LOG(ERROR, log_context_) << "No KAnon service configured.";
    benchmarking_logger_->End();
    FinishWithStatus(grpc::Status(grpc::FAILED_PRECONDITION, kMissingInputs));
    return;
  }

  // An enum type will always have a value in the message received. This will
  // default to 0/Unknown or "" if not explicitly set.
  auto browser = raw_request_.browser();
  auto instance_hash = raw_request_.instance_hash();

  if (browser == "") {
    // This is unlikely to happen since we already have this checked in gRPC
    PS_LOG(ERROR, log_context_) << "No KAnon join browser provided.";
    benchmarking_logger_->End();
    FinishWithStatus(grpc::Status(grpc::INVALID_ARGUMENT, kMissingInputs));
    return;
  }

  // Our instance_hash must be provided
  if (instance_hash == "") {
    // This is unlikely to happen since we already have this checked in gRPC
    PS_LOG(ERROR, log_context_) << "No KAnon join instance_hash provided.";
    benchmarking_logger_->End();
    FinishWithStatus(grpc::Status(grpc::INVALID_ARGUMENT, kMissingInputs));
    return;
  }

  // Log received fields.
  PS_VLOG(kPlain, log_context_)
      << "KAnon browser instance: " << browser << "\n";
  PS_VLOG(kPlain, log_context_) << "KAnon hash: " << instance_hash << "\n";

  auto join_raw_request = std::make_unique<
      microsoft::k_anonymity::proto::JoinRequest::JoinRawRequest>();
  join_raw_request->set_data_type(
      microsoft::k_anonymity::proto::KANONDataType::CREATIVE);

  join_raw_request->set_browser(browser);
  join_raw_request->set_instance_hash(instance_hash);

  PS_VLOG(kPlain) << "Raw K-Anon JoinRequest:\n"
                  << join_raw_request->ShortDebugString();

  grpc::ClientContext* client_context =
      client_contexts_.Add(microsoft_k_anon_metadata_);
  absl::Status execute_result =
      microsoft_kanon_join_async_client_->ExecuteInternal(
          std::move(join_raw_request), client_context,
          [](absl::StatusOr<std::unique_ptr<google::protobuf::Empty>>
                 raw_response,
             ResponseMetadata response_metadata) {
            // Join has a no-op/Empty response.
            return absl::OkStatus();
          },
          absl::Milliseconds(config_.microsoft_k_anon_timeout_ms));

  if (!execute_result.ok()) {
    LogIfError(
        metric_context_->AccumulateMetric<metric::kBfeErrorCountByErrorCode>(
            1, metric::kBfeGenerateProtectedAppSignalsBidsFailedToCall));
    PS_LOG(ERROR, log_context_) << "Failed to make async K-Anon Join call: "
                                   "(error: "
                                << execute_result.ToString() << ")";
  }

  // For now we will just finish the response and say things went ok as we don't
  // care about the response.
  FinishWithStatus(grpc::Status::OK);
}

absl::btree_map<std::string, std::string>
MicrosoftJoinKAnonReactor::GetLoggingContext() {
  const auto& log_context = raw_request_.log_context();
  return {{kGenerationId, log_context.generation_id()},
          {kBuyerDebugId, log_context.adtech_debug_id()}};
}

void MicrosoftJoinKAnonReactor::FinishWithStatus(const grpc::Status& status) {
  if (status.error_code() != grpc::StatusCode::OK) {
    metric_context_->SetRequestResult(server_common::ToAbslStatus(status));
  }
  Finish(status);
}

// Deletes all data related to this object.
void MicrosoftJoinKAnonReactor::OnDone() { delete this; }

}  // namespace privacy_sandbox::bidding_auction_servers
