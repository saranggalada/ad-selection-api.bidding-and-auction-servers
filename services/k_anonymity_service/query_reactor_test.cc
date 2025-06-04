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

#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "services/common/constants/common_service_flags.h"
#include "services/common/encryption/key_fetcher_factory.h"
#include "services/common/encryption/mock_crypto_client_wrapper.h"
#include "services/common/metric/server_definition.h"
#include "services/common/test/mocks.h"
#include "services/common/test/random.h"
#include "services/common/test/utils/test_init.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_benchmarking_logger.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_no_op_logger.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::MockCryptoClientWrapper;
using privacy_sandbox::bidding_auction_servers::TrustedServersConfigClient;
using privacy_sandbox::bidding_auction_servers::metric::MetricContextMap;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using privacy_sandbox::server_common::PrivateKey;
using privacy_sandbox::server_common::telemetry::BuildDependentConfig;
using proto::KANONDataType;

namespace {

constexpr char kKeyId[] = "key_id";
constexpr char kSecret[] = "secret";

using Request = QueryRequest;
using RawRequest = QueryRequest::QueryRawRequest;
using Response = QueryResponse;
using RawResponse = QueryResponse::QueryRawResponse;

using testing::AnyNumber;
void SetupMockCryptoClientWrapper(MockCryptoClientWrapper& crypto_client) {
  EXPECT_CALL(crypto_client, HpkeEncrypt)
      .Times(AnyNumber())
      .WillRepeatedly(
          [](const google::cmrt::sdk::public_key_service::v1::PublicKey& key,
             const std::string& plaintext_payload) {
            google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse
                hpke_encrypt_response;
            hpke_encrypt_response.set_secret(kSecret);
            hpke_encrypt_response.mutable_encrypted_data()->set_key_id(kKeyId);
            hpke_encrypt_response.mutable_encrypted_data()->set_ciphertext(
                plaintext_payload);
            return hpke_encrypt_response;
          });

  // Mock the HpkeDecrypt() call on the crypto_client. This is used by the
  // service to decrypt the incoming request.
  EXPECT_CALL(crypto_client, HpkeDecrypt)
      .Times(AnyNumber())
      .WillRepeatedly(
          [](const PrivateKey& private_key, const std::string& ciphertext) {
            google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse
                hpke_decrypt_response;
            *hpke_decrypt_response.mutable_payload() = ciphertext;
            hpke_decrypt_response.set_secret(kSecret);
            return hpke_decrypt_response;
          });

  // Mock the AeadEncrypt() call on the crypto_client. This is used to encrypt
  // the response coming back from the service.
  EXPECT_CALL(crypto_client, AeadEncrypt)
      .Times(AnyNumber())
      .WillRepeatedly(
          [](const std::string& plaintext_payload, const std::string& secret) {
            google::cmrt::sdk::crypto_service::v1::AeadEncryptedData data;
            *data.mutable_ciphertext() = plaintext_payload;
            google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse
                aead_encrypt_response;
            *aead_encrypt_response.mutable_encrypted_data() = std::move(data);
            return aead_encrypt_response;
          });
}

class QueryReactorTest : public testing::Test {
 protected:
  void SetUp() override {
    // initialize
    privacy_sandbox::bidding_auction_servers::CommonTestInit();

    privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(
        privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
    auto dependent_config =
        std::make_unique<BuildDependentConfig>(config_proto);
    MetricContextMap<QueryRequest>(std::move(dependent_config))->Get(&request_);

    TrustedServersConfigClient config_client({});
    config_client.SetOverride(
        privacy_sandbox::bidding_auction_servers::kTrue,
        privacy_sandbox::bidding_auction_servers::TEST_MODE);
    key_fetcher_manager_ = CreateKeyFetcherManager(
        config_client, /* public_key_fetcher= */ nullptr);
    SetupMockCryptoClientWrapper(*crypto_client_);
    request_.set_key_id(kKeyId);

    privacy_sandbox::server_common::log::SetGlobalPSVLogLevel(99);
  }

  void CheckQuery(const RawRequest& raw_request,
                  const Response& expected_response) {
    Response response;
    std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarkingLogger =
        std::make_unique<KAnonymityNoOpLogger>();
    KAnonymityServiceRuntimeConfig runtime_config = {};
    request_.set_request_ciphertext(raw_request.SerializeAsString());
    QueryReactor reactor(
        &context_, &request_, &response, std::move(benchmarkingLogger),
        key_fetcher_manager_.get(), crypto_client_.get(), runtime_config);
    reactor.Execute();

    // Validate the response is as we expect it.
    google::protobuf::util::MessageDifferencer diff;
    std::string diff_output;
    diff.ReportDifferencesToString(&diff_output);

    QueryResponse::QueryRawResponse raw_response;
    raw_response.ParseFromString(response.response_ciphertext());

    QueryResponse::QueryRawResponse expected_raw_response;
    expected_raw_response.ParseFromString(
        expected_response.response_ciphertext());

    EXPECT_TRUE(diff.Compare(expected_raw_response, raw_response))
        << diff_output;
  }

  grpc::CallbackServerContext context_;
  Request request_;
  std::unique_ptr<MockCryptoClientWrapper> crypto_client_ =
      std::make_unique<MockCryptoClientWrapper>();
  std::unique_ptr<KeyFetcherManagerInterface> key_fetcher_manager_;
};

TEST_F(QueryReactorTest, SingleHashReturnsResult) {
  RawResponse raw_query_response;
  raw_query_response.add_is_k_anonymous(false);

  // Create a simple request with a single hash which we expect to return a
  // single k_anon result.
  RawRequest raw_request;
  raw_request.set_data_type(KANONDataType::CREATIVE);
  raw_request.add_instance_hash("hash_1");

  Response query_response;
  *query_response.mutable_response_ciphertext() =
      raw_query_response.SerializeAsString();
  CheckQuery(raw_request, query_response);
}

TEST_F(QueryReactorTest, MultipleHashesReturnsResults) {
  RawResponse raw_query_response;

  // Create a simple request with a few requests hash which we expect to return
  // a single k_anon result for each request.
  RawRequest raw_request;
  raw_request.set_data_type(KANONDataType::CREATIVE);

  int query_count = 50;
  for (int i = 1; i <= query_count; i++) {
    raw_request.add_instance_hash("hash_" + std::to_string(i));
    raw_query_response.add_is_k_anonymous(false);
  }

  Response query_response;
  *query_response.mutable_response_ciphertext() =
      raw_query_response.SerializeAsString();
  CheckQuery(raw_request, query_response);
}

}  // namespace
}  // namespace microsoft::k_anonymity
