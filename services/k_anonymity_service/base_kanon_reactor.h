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

#ifndef SERVICES_K_ANONYMITY_SERVICE_BASE_KANON_REACTOR_H_
#define SERVICES_K_ANONYMITY_SERVICE_BASE_KANON_REACTOR_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "services/common/constants/user_error_strings.h"
#include "services/common/encryption/crypto_client_wrapper_interface.h"
#include "services/common/util/request_response_constants.h"
#include "services/k_anonymity_service/data/runtime_config.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/logger/request_context_impl.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::CryptoClientWrapperInterface;
using privacy_sandbox::bidding_auction_servers::kEmptyCiphertextError;
using privacy_sandbox::bidding_auction_servers::kEmptyKeyIdError;
using privacy_sandbox::bidding_auction_servers::kInvalidKeyIdError;
using privacy_sandbox::bidding_auction_servers::kMalformedCiphertext;
using privacy_sandbox::bidding_auction_servers::RequestLogContext;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using privacy_sandbox::server_common::PrivateKey;

template <typename Request, typename RawRequest, typename Response,
          typename RawResponse>
class BaseKAnonReactor : public grpc::ServerUnaryReactor {
 public:
  explicit BaseKAnonReactor(
      grpc::CallbackServerContext* context,
      const KAnonymityServiceRuntimeConfig& runtime_config,
      const Request* request, Response* response,
      KeyFetcherManagerInterface* key_fetcher_manager,
      CryptoClientWrapperInterface* crypto_client)
      : context_(context),
        request_(request),
        response_(response),
        runtime_config_(runtime_config),
        key_fetcher_manager_(key_fetcher_manager),
        crypto_client_(crypto_client),
        log_context_(GetLoggingContext(this->raw_request_),
                     this->raw_request_.consented_debug_config(), [this]() {
                       return this->raw_response_.mutable_debug_info();
                     }) {
    PS_VLOG(5) << "Encryption is enabled, decrypting request now";
    if (DecryptRequest()) {
      PS_VLOG(5) << "Decrypted request: " << raw_request_.DebugString();
    } else {
      PS_LOG(ERROR) << "Failed to decrypt the request";
    }
  }

  virtual ~BaseKAnonReactor() = default;

  virtual void Execute() = 0;

 protected:
  // Gets logging context as key/value pair that is useful for tracking a
  // request through the B&A services.
  absl::btree_map<std::string, std::string> GetLoggingContext(
      const RawRequest& kanon_request) {
    const auto& logging_context = kanon_request.log_context();

    return {{privacy_sandbox::bidding_auction_servers::kGenerationId,
             logging_context.generation_id()},
            {privacy_sandbox::bidding_auction_servers::kAdtechDebugId,
             logging_context.adtech_debug_id()}};
  }

  // Cleans up all state associated with the BaseKAnonReactor.
  // Called only after the grpc request is finalized and finished.
  void OnDone() override { delete this; };

  // Handles early-cancellation by the client.
  void OnCancel() override {
    PS_LOG(ERROR) << "Request cancelled/aborted by client.";
  };

  // Decrypts the request ciphertext in and returns whether decryption was
  // successful. If successful, the result is written into 'raw_request_'.
  bool DecryptRequest() {
    if (request_->key_id().empty()) {
      PS_LOG(ERROR) << "No key ID found in the request";
      Finish(
          grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, kEmptyKeyIdError));
      return false;
    } else if (request_->request_ciphertext().empty()) {
      PS_LOG(ERROR) << "No ciphertext found in the request";
      Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          kEmptyCiphertextError));
      return false;
    }

    std::optional<PrivateKey> private_key =
        key_fetcher_manager_->GetPrivateKey(request_->key_id());
    if (!private_key.has_value()) {
      PS_LOG(ERROR)
          << "Unable to fetch private key from the key fetcher manager";
      Finish(
          grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, kInvalidKeyIdError));
      return false;
    }

    absl::StatusOr<google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>
        decrypt_response = crypto_client_->HpkeDecrypt(
            *private_key, request_->request_ciphertext());
    if (!decrypt_response.ok()) {
      PS_LOG(ERROR) << "Unable to decrypt the request ciphertext: "
                    << decrypt_response.status();
      Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          kMalformedCiphertext));
      return false;
    }

    hpke_secret_ = std::move(*decrypt_response->mutable_secret());
    return raw_request_.ParseFromString(decrypt_response->payload());
  }

  // Encrypts `raw_response` and sets the result on the 'response_ciphertext'
  // field in the response. Returns whether encryption was successful.
  bool EncryptResponse() {
    std::string payload = raw_response_.SerializeAsString();
    absl::StatusOr<google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse>
        aead_encrypt = crypto_client_->AeadEncrypt(payload, hpke_secret_);
    if (!aead_encrypt.ok()) {
      PS_LOG(ERROR) << "AEAD encrypt failed: " << aead_encrypt.status();
      Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                          aead_encrypt.status().ToString()));
      return false;
    }

    response_->set_response_ciphertext(
        aead_encrypt->encrypted_data().ciphertext());
    return true;
  }

  virtual void FinishWithStatus(const grpc::Status& status) {
    if (status.error_code() != grpc::StatusCode::OK) {
      PS_LOG(ERROR, log_context_) << "RPC failed: " << status.error_message();
    }

    Finish(status);
  }

  grpc::CallbackServerContext* context_;

  // The client request, lifecycle managed by gRPC.
  const Request* request_;
  RawRequest raw_request_;
  // The client response, lifecycle managed by gRPC.
  Response* response_;
  RawResponse raw_response_;

  const KAnonymityServiceRuntimeConfig& runtime_config_;

  // Variables used to fetch the TEE private key and store the HPKE secret
  // during a requests lifetime as both decryption of the request and encryption
  // of the response will leverage this.
  KeyFetcherManagerInterface* key_fetcher_manager_;
  CryptoClientWrapperInterface* crypto_client_;
  std::string hpke_secret_;

  RequestLogContext log_context_;
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_K_ANONYMITY_SERVICE_BASE_KANON_REACTOR_H_
