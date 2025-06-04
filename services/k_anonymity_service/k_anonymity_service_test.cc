//  Copyright (c) Microsoft Corporation
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

#include "services/k_anonymity_service/k_anonymity_service.h"

#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/synchronization/blocking_counter.h"
#include "api/microsoft_k_anonymity.grpc.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "services/common/constants/common_service_flags.h"
#include "services/common/encryption/key_fetcher_factory.h"
#include "services/common/encryption/mock_crypto_client_wrapper.h"
#include "services/common/metric/server_definition.h"
#include "services/common/test/mocks.h"
#include "services/common/test/utils/service_utils.h"
#include "services/common/test/utils/test_init.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_no_op_logger.h"
#include "services/k_anonymity_service/join_reactor.h"
#include "services/k_anonymity_service/query_reactor.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace microsoft::k_anonymity {
namespace {
constexpr char kKeyId[] = "key_id";

using ::google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using privacy_sandbox::bidding_auction_servers::CreateServiceStub;
using privacy_sandbox::bidding_auction_servers::LocalServiceStartResult;
using privacy_sandbox::bidding_auction_servers::MockCryptoClientWrapper;
using privacy_sandbox::bidding_auction_servers::StartLocalService;
using privacy_sandbox::bidding_auction_servers::TrustedServersConfigClient;
using privacy_sandbox::bidding_auction_servers::metric::MetricContextMap;
using privacy_sandbox::server_common::KeyFetcherManagerInterface;
using privacy_sandbox::server_common::telemetry::BuildDependentConfig;
using privacy_sandbox::server_common::telemetry::TelemetryConfig;
using proto::JoinRequest;
using proto::KANONDataType;
using proto::KAnonymity;
using proto::QueryRequest;
using proto::QueryResponse;
using ::testing::NiceMock;

QueryRequest::QueryRawRequest CreateQueryRawRequest() {
  QueryRequest::QueryRawRequest raw_request;
  raw_request.set_data_type(KANONDataType::CREATIVE);
  raw_request.add_instance_hash("hash_1");

  PS_LOG(INFO) << "Created query request:\n" << raw_request.DebugString();
  return raw_request;
}

QueryRequest CreateQueryRequest(
    const QueryRequest::QueryRawRequest& raw_request) {
  QueryRequest request;
  request.set_request_ciphertext(raw_request.SerializeAsString());
  request.set_key_id(kKeyId);
  return request;
}

JoinRequest::JoinRawRequest CreateJoinRawRequest() {
  JoinRequest::JoinRawRequest raw_request;
  raw_request.set_browser("id123");
  raw_request.set_data_type(KANONDataType::CREATIVE);
  raw_request.set_instance_hash("hash_1");

  PS_LOG(INFO) << "Created join request:\n" << raw_request.DebugString();
  return raw_request;
}

JoinRequest CreateJoinRequest(const JoinRequest::JoinRawRequest& raw_request) {
  JoinRequest request;
  request.set_request_ciphertext(raw_request.SerializeAsString());
  request.set_key_id(kKeyId);
  return request;
}

class KAnonymityServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    privacy_sandbox::bidding_auction_servers::CommonTestInit();

    config_.SetOverride(privacy_sandbox::bidding_auction_servers::kTrue,
                        privacy_sandbox::bidding_auction_servers::TEST_MODE);

    TelemetryConfig config_proto;
    config_proto.set_mode(TelemetryConfig::PROD);
    auto dependent_config =
        std::make_unique<BuildDependentConfig>(config_proto);
    MetricContextMap<QueryRequest>(std::move(dependent_config));
    dependent_config = std::make_unique<BuildDependentConfig>(config_proto);
    MetricContextMap<JoinRequest>(std::move(dependent_config));

    privacy_sandbox::server_common::log::SetGlobalPSVLogLevel(99);
  }

  KAnonymityService CreateKAnonymityService() {
    return KAnonymityService(
        [](grpc::CallbackServerContext* context, const QueryRequest* request,
           QueryResponse* response,
           KeyFetcherManagerInterface* key_fetcher_manager,
           CryptoClientWrapperInterface* crypto_client,
           const KAnonymityServiceRuntimeConfig& runtime_config) {
          std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarkingLogger =
              std::make_unique<KAnonymityNoOpLogger>();
          auto query_reactor = std::make_unique<QueryReactor>(
              context, request, response, std::move(benchmarkingLogger),
              key_fetcher_manager, crypto_client, runtime_config);
          return query_reactor.release();
        },
        [](grpc::CallbackServerContext* context, const JoinRequest* request,
           const google::protobuf::Empty* response,
           KeyFetcherManagerInterface* key_fetcher_manager,
           CryptoClientWrapperInterface* crypto_client,
           const KAnonymityServiceRuntimeConfig& runtime_config) {
          std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarkingLogger =
              std::make_unique<KAnonymityNoOpLogger>();
          auto join_reactor = std::make_unique<JoinReactor>(
              context, request, response, std::move(benchmarkingLogger),
              key_fetcher_manager, crypto_client, runtime_config);
          return join_reactor.release();
        },
        CreateKeyFetcherManager(config_, /*public_key_fetcher=*/nullptr),
        std::move(crypto_client_), runtime_config_);
  }

  std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarking_logger_ =
      std::make_unique<KAnonymityNoOpLogger>();
  TrustedServersConfigClient config_{{}};
  KAnonymityServiceRuntimeConfig runtime_config_;
  std::unique_ptr<NiceMock<MockCryptoClientWrapper>> crypto_client_ =
      std::make_unique<NiceMock<MockCryptoClientWrapper>>();
};

TEST_F(KAnonymityServiceTest, UnsetQueryDataTypeFailsRPC) {
  // Setup a service and use test helpers to start it locally.
  auto kanon_service = CreateKAnonymityService();
  LocalServiceStartResult start_service_result =
      StartLocalService(&kanon_service);
  std::unique_ptr<KAnonymity::StubInterface> stub =
      CreateServiceStub<KAnonymity>(start_service_result.port);
  grpc::ClientContext context;

  // Create a Query request with all fields filled out except a proper DataType.
  // We should get a rejection back from RPC.
  QueryRequest::QueryRawRequest raw_request = CreateQueryRawRequest();
  raw_request.clear_data_type();

  PS_LOG(INFO) << "Modified query request:\n" << raw_request.ShortDebugString();

  auto request = CreateQueryRequest(raw_request);
  QueryResponse response;
  grpc::Status status = stub->Query(&context, request, &response);

  ASSERT_FALSE(status.ok());
}

TEST_F(KAnonymityServiceTest, UnsetQueryInstancesTypeFailsRPC) {
  // Setup a service and use test helpers to start it locally.
  auto kanon_service = CreateKAnonymityService();
  LocalServiceStartResult start_service_result =
      StartLocalService(&kanon_service);
  std::unique_ptr<KAnonymity::StubInterface> stub =
      CreateServiceStub<KAnonymity>(start_service_result.port);
  grpc::ClientContext context;

  // Create a Query request with all fields filled out except a proper set of
  // instances. We should get a rejection back from RPC.
  QueryRequest::QueryRawRequest raw_request = CreateQueryRawRequest();
  raw_request.clear_instance_hash();

  auto request = CreateQueryRequest(raw_request);
  QueryResponse response;
  grpc::Status status = stub->Query(&context, request, &response);

  ASSERT_FALSE(status.ok());
}

TEST_F(KAnonymityServiceTest, UnsetJoinBrowserFailsRPC) {
  // Setup a service and use test helpers to start it locally.
  auto kanon_service = CreateKAnonymityService();
  LocalServiceStartResult start_service_result =
      StartLocalService(&kanon_service);
  std::unique_ptr<KAnonymity::StubInterface> stub =
      CreateServiceStub<KAnonymity>(start_service_result.port);
  grpc::ClientContext context;

  // Create a Join request with all fields filled out except a proper browser.
  // We should get a rejection back from RPC.
  JoinRequest::JoinRawRequest raw_request = CreateJoinRawRequest();
  raw_request.clear_browser();

  auto request = CreateJoinRequest(raw_request);
  google::protobuf::Empty response;
  grpc::Status status = stub->Join(&context, request, &response);

  ASSERT_FALSE(status.ok());
}

TEST_F(KAnonymityServiceTest, UnsetJoinDataTypeFailsRPC) {
  // Setup a service and use test helpers to start it locally.
  auto kanon_service = CreateKAnonymityService();
  LocalServiceStartResult start_service_result =
      StartLocalService(&kanon_service);
  std::unique_ptr<KAnonymity::StubInterface> stub =
      CreateServiceStub<KAnonymity>(start_service_result.port);
  grpc::ClientContext context;

  // Create a Join request with all fields filled out except a proper DataType.
  // We should get a rejection back from RPC.
  JoinRequest::JoinRawRequest raw_request = CreateJoinRawRequest();
  raw_request.clear_data_type();

  auto request = CreateJoinRequest(raw_request);
  google::protobuf::Empty response;
  grpc::Status status = stub->Join(&context, request, &response);

  ASSERT_FALSE(status.ok());
}

TEST_F(KAnonymityServiceTest, UnsetJoinInstancesTypeFailsRPC) {
  // Setup a service and use test helpers to start it locally.
  auto kanon_service = CreateKAnonymityService();
  LocalServiceStartResult start_service_result =
      StartLocalService(&kanon_service);
  std::unique_ptr<KAnonymity::StubInterface> stub =
      CreateServiceStub<KAnonymity>(start_service_result.port);
  grpc::ClientContext context;

  // Create a Join request with all fields filled out except a proper set of
  // instances. We should get a rejection back from RPC.
  JoinRequest::JoinRawRequest raw_request = CreateJoinRawRequest();
  raw_request.clear_instance_hash();

  auto request = CreateJoinRequest(raw_request);
  google::protobuf::Empty response;
  grpc::Status status = stub->Join(&context, request, &response);

  ASSERT_FALSE(status.ok());
}

}  // namespace
}  // namespace microsoft::k_anonymity
