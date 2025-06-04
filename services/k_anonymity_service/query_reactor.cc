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

#include "services/k_anonymity_service/query_reactor.h"

#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "services/common/util/json_util.h"
#include "services/common/util/request_response_constants.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::kEncrypted;
using privacy_sandbox::bidding_auction_servers::kInternalServerError;
using privacy_sandbox::bidding_auction_servers::kMissingInputs;
using privacy_sandbox::bidding_auction_servers::kPlain;
using privacy_sandbox::bidding_auction_servers::metric::
    KAnonymityQueryContextMap;
using proto::KANONDataType;

QueryReactor::QueryReactor(
    grpc::CallbackServerContext* context, const QueryRequest* request,
    QueryResponse* response,
    std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarking_logger,
    KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client,
    const KAnonymityServiceRuntimeConfig& runtime_config)
    : BaseKAnonReactor<QueryRequest, QueryRequest::QueryRawRequest,
                       QueryResponse, QueryResponse::QueryRawResponse>(
          context, runtime_config, request, response, key_fetcher_manager,
          crypto_client),
      benchmarking_logger_(std::move(benchmarking_logger)) {
  CHECK_OK([this]() {
    PS_ASSIGN_OR_RETURN(metric_context_,
                        KAnonymityQueryContextMap()->Remove(request_));
    if (log_context_.is_consented()) {
      metric_context_->SetConsented(raw_request_.log_context().generation_id());
    }
    return absl::OkStatus();
  }()) << "KAnonymityQueryContextMap()->Get(request) should have been called";
}

void QueryReactor::Execute() {
  benchmarking_logger_->BuildInputBegin();

  PS_VLOG(kEncrypted, log_context_) << "Encrypted KAnon QueryRequest:\n"
                                    << request_->ShortDebugString();
  PS_VLOG(kPlain, log_context_) << "QueryRawRequest:\n"
                                << raw_request_.ShortDebugString();

  // An enum type will always have a value in the message received. This will
  // default to 0/Unknown if not explicitly set.
  auto data_type = raw_request_.data_type();

  if (data_type == KANONDataType::UNKNOWN) {
    // This is unlikely to happen since we already have this checked in gRPC
    PS_LOG(ERROR, log_context_) << "No KAnon query DataType provided.";
    benchmarking_logger_->End();
    FinishWithStatus(grpc::Status(grpc::INVALID_ARGUMENT, kMissingInputs));
    return;
  }

  // Our instance_hash is guaranteed to exist but may be an empty set.
  if (raw_request_.instance_hash_size() <= 0) {
    // This is unlikely to happen since we already have this checked in gRPC
    PS_LOG(ERROR, log_context_) << "No KAnon query instance_hash provided.";
    benchmarking_logger_->End();
    FinishWithStatus(grpc::Status(grpc::INVALID_ARGUMENT, kMissingInputs));
    return;
  }

  // Given the protobuf and gRPC contracts we are guaranteed to have these data
  // members present at this point.
  auto instance_hashes = raw_request_.instance_hash();

  // Log received fields.
  PS_VLOG(kPlain, log_context_) << "KAnon Data Type: " << data_type << "\n";
  for (const auto& query_hash : instance_hashes) {
    PS_VLOG(kPlain, log_context_)
        << "KAnon hash requested query: " << query_hash.data() << "\n";

    // For now we'll just say false to all requests!
    raw_response_.add_is_k_anonymous(false);
  }

  // For now we will just finish the response and say things went ok.
  // task.ms/<ID>: Add KAnon Query Base Logic.
  EncryptResponseAndFinish(grpc::Status::OK);
}

void QueryReactor::EncryptResponseAndFinish(grpc::Status status) {
  PS_VLOG(kPlain, log_context_) << "QueryRawResponse:\n"
                                << raw_response_.ShortDebugString();

  if (!EncryptResponse()) {
    PS_LOG(ERROR, log_context_)
        << "Failed to encrypt the KAnon query response.";
    status = grpc::Status(grpc::INTERNAL, kInternalServerError);
  }
  if (status.error_code() != grpc::StatusCode::OK) {
    metric_context_->SetRequestResult(
        privacy_sandbox::server_common::ToAbslStatus(status));
  }
  PS_VLOG(kEncrypted, log_context_) << "Encrypted QueryResponse\n"
                                    << response_->ShortDebugString();
  Finish(status);
}

void QueryReactor::OnDone() { delete this; }

}  // namespace microsoft::k_anonymity
