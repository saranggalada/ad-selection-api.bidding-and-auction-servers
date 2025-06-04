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

#include "services/common/clients/microsoft_k_anon_server/k_anon_async_client.h"

#include "services/common/util/request_response_constants.h"
#include "src/public/cpio/interface/crypto_client/crypto_client_interface.h"

namespace microsoft::k_anonymity {

using ::google::cmrt::sdk::public_key_service::v1::PublicKey;
using privacy_sandbox::bidding_auction_servers::kPlain;

namespace {

template <typename Request, typename Response, typename RawResponse>
void OnRpcDone(
    const grpc::Status& status,
    RawClientParams<Request, Response, RawResponse>* params,
    std::function<absl::StatusOr<std::unique_ptr<RawResponse>>(Response*)>
        decrypt_response) {
  if (!status.ok()) {
    PS_LOG(ERROR) << "SendRPC completion status not ok: "
                  << privacy_sandbox::server_common::ToAbslStatus(status);
    params->OnDone(status);
    return;
  }
  PS_VLOG(6) << "SendRPC completion status ok";
  auto decrypted_response = decrypt_response(params->ResponseRef());
  if (!decrypted_response.ok()) {
    PS_LOG(ERROR) << "KAnonAsyncGrpcClient failed to decrypt response";
    params->OnDone(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                decrypted_response.status().ToString()));
    return;
  }

  params->SetRawResponse(*std::move(decrypted_response));
  PS_VLOG(6) << "Returning the decrypted response via callback";
  params->OnDone(status);
}

}  // namespace

std::unique_ptr<proto::QueryRequest::QueryRawRequest> CreateBidsQueryRawRequest(
    const GetBidsResponse::GetBidsRawResponse& get_bid_raw_response,
    const LogContext& log_context) {
  auto query_raw_request =
      std::make_unique<proto::QueryRequest::QueryRawRequest>();
  query_raw_request->set_data_type(proto::KANONDataType::CREATIVE);
  for (const auto& ad : get_bid_raw_response.bids()) {
    query_raw_request->add_instance_hash(GetKAnonHash(ad.render()));
  }

  for (const auto& ad : get_bid_raw_response.protected_app_signals_bids()) {
    query_raw_request->add_instance_hash(GetKAnonHash(ad.render()));
  }

  PS_VLOG(5) << "Added " << query_raw_request->instance_hash_size()
             << " bids to check K-Anon.";

  PS_VLOG(kPlain) << "Raw K-Anon QueryRequest:\n"
                  << query_raw_request->ShortDebugString();

  return query_raw_request;
}

void AttachBiddingMetadata(
    absl::StatusOr<std::unique_ptr<proto::QueryResponse::QueryRawResponse>>
        query_raw_response,
    GetBidsResponse::GetBidsRawResponse& get_bid_raw_response) {
  PS_VLOG(5) << "K-Anon AttachBiddingMetadata";

  if (!query_raw_response.ok()) {
    return;
  }

  PS_VLOG(kPlain) << "Raw K-Anon QueryResponse:\n"
                  << (*query_raw_response)->ShortDebugString();

  // We should have exactly as many responses as we have bids.
  CHECK((*query_raw_response)->is_k_anonymous_size() ==
        (get_bid_raw_response.bids_size() +
         get_bid_raw_response.protected_app_signals_bids_size()));

  // We know the response comes back in the same order we queried and that we
  // queried with bids then protected_app_signals_bids. We will iterate through
  // both now and attach our k-anon status!
  int count = 0;
  for (auto& ad : *(get_bid_raw_response.mutable_bids())) {
    ad.set_microsoft_is_k_anon((*query_raw_response)->is_k_anonymous(count));
    count++;
  }

  for (auto& ad :
       *(get_bid_raw_response.mutable_protected_app_signals_bids())) {
    ad.set_microsoft_is_k_anon((*query_raw_response)->is_k_anonymous(count));
    count++;
  }

  PS_VLOG(kPlain) << "Raw updated bid response:\n"
                  << get_bid_raw_response.ShortDebugString();
}

std::string GetKAnonHash(const std::string& ad_creative_url) {
  // We know k-anon will be isolated per-buyer so we only need to ensure the URL
  // is not too distinct; this is what will potentially be rendered on a user's
  // device.
  return std::to_string(std::hash<std::string>{}(ad_creative_url));
}

KAnonQueryAsyncGrpcClient::KAnonQueryAsyncGrpcClient(
    KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client, proto::KAnonymity::Stub* stub)
    : DefaultAsyncGrpcClient(key_fetcher_manager, crypto_client), stub_(stub) {}

void KAnonQueryAsyncGrpcClient::SendRpc(
    const std::string& hpke_secret, grpc::ClientContext* context,
    RawClientParams<proto::QueryRequest, proto::QueryResponse,
                    proto::QueryResponse::QueryRawResponse>* params) const {
  PS_VLOG(5) << "KAnonQueryAsyncGrpcClient SendRpc invoked ...";
  stub_->async()->Query(
      context, params->RequestRef(), params->ResponseRef(),
      [this, params, hpke_secret](const grpc::Status& status) {
        OnRpcDone<proto::QueryRequest, proto::QueryResponse,
                  proto::QueryResponse::QueryRawResponse>(
            status, params,
            [this, &hpke_secret](proto::QueryResponse* response) {
              return DecryptResponse(hpke_secret, response);
            });
      });
}

KAnonJoinAsyncGrpcClient::KAnonJoinAsyncGrpcClient(
    KeyFetcherManagerInterface* key_fetcher_manager,
    CryptoClientWrapperInterface* crypto_client, proto::KAnonymity::Stub* stub)
    : DefaultAsyncGrpcClient(key_fetcher_manager, crypto_client), stub_(stub) {}

void KAnonJoinAsyncGrpcClient::SendRpc(
    const std::string& hpke_secret, grpc::ClientContext* context,
    RawClientParams<proto::JoinRequest, google::protobuf::Empty,
                    google::protobuf::Empty>* params) const {
  PS_VLOG(5) << "KAnonJoinAsyncGrpcClient SendRpc invoked ...";
  stub_->async()->Join(context, params->RequestRef(), params->ResponseRef(),
                       [params, hpke_secret](const grpc::Status& status) {
                         OnRpcDone<proto::JoinRequest, google::protobuf::Empty,
                                   google::protobuf::Empty>(
                             status, params,
                             [](google::protobuf::Empty* response) {
                               // Join has a no-op/Empty response.
                               return absl::OkStatus();
                             });
                       });
}

}  // namespace microsoft::k_anonymity
