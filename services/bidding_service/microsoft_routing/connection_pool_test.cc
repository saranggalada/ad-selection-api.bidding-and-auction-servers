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

#ifndef CONNECTION_POOL_TEST_H_
#define CONNECTION_POOL_TEST_H_

#include "services/bidding_service/microsoft_routing/connection_pool.h"

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "api/microsoft_routing_utils.grpc.pb.h"
#include "api/microsoft_routing_utils.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "services/bidding_service/runtime_flags.h"
#include "services/common/clients/client_params.h"
#include "services/common/clients/config/trusted_server_config_client.h"
#include "services/common/constants/common_service_flags.h"
#include "services/common/encryption/crypto_client_factory.h"
#include "services/common/encryption/key_fetcher_factory.h"
#include "services/common/encryption/mock_crypto_client_wrapper.h"

namespace privacy_sandbox::bidding_auction_servers {
// constexpr char kTestSecret[] = "secret";
// constexpr char kTestKeyId[] = "keyid";

using ::testing::AnyNumber;

template <typename Request, typename RawRequest, typename Response,
          typename RawResponse, typename ServiceThread, typename TestClient,
          typename ClientConfig, typename Service>
struct AsyncGrpcClientTypeDefinitions {
  using RequestType = Request;
  using RawRequestType = RawRequest;
  using ResponseType = Response;
  using RawResponseType = RawResponse;
  using ServiceThreadType = ServiceThread;
  using TestClientType = TestClient;
  using ClientConfigType = ClientConfig;
  using ServiceType = Service;
};

template <class ConnectionpoolType>
class ConnectionPoolTest : public ::testing::Test {
 protected:
  void SetUp() override { config_client_.SetFlagForTest(kTrue, TEST_MODE); }

  TrustedServersConfigClient config_client_{{}};
};

TYPED_TEST_SUITE_P(ConnectionPoolTest);

TYPED_TEST_P(ConnectionPoolTest, CallsServerWithRequest) {
  using ServiceThread = typename TypeParam::ServiceThreadType;
  using Request = typename TypeParam::RequestType;
  using RawRequest = typename TypeParam::RawRequestType;
  using Response = typename TypeParam::ResponseType;

  // using RawResponse = typename TypeParam::RawResponseType;
  // using TestClient = typename TypeParam::TestClientType;
  // using ClientConfig = typename TypeParam::ClientConfigType;
  // using ServiceType = typename TypeParam::ServiceType;

  RawRequest received_request;
  auto dummy_service_thread_ = std::make_unique<ServiceThread>(
      [&received_request](grpc::CallbackServerContext* context,
                          const Request* request, Response* response) {
        received_request.ParseFromString(request->request_ciphertext());
        auto reactor = context->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
      });

  // ClientConfig client_config = {
  //     .tee_kv_server_addr = dummy_service_thread_->GetServerAddr(),
  // };
  RawRequest raw_request;
  auto input_request_ptr = std::make_unique<RawRequest>(raw_request);
  MockCryptoClientWrapper crypto_client;
  SetupMockCryptoClientWrapper(raw_request, crypto_client);
  auto key_fetcher_manager = CreateKeyFetcherManager(
      this->config_client_, /* public_key_fetcher= */ nullptr);

  privacy_sandbox::bidding_auction_servers::BiddingServiceRuntimeConfig
      runtime_config = {
          .tee_kv_server_addr = dummy_service_thread_->GetServerAddr(),
      };

  microsoft::routing_utils::ConnectionPool connection_pool_(
      runtime_config, key_fetcher_manager.get(), &crypto_client);
  microsoft::routing_utils::proto::KVServerInfo serverInfo;
  serverInfo.set_server_name("KV_SERVER");

  auto kv_async_client_ = connection_pool_.GetAsyncClient(serverInfo);
  EXPECT_NE(kv_async_client_.ok(), false);
  EXPECT_NE(*kv_async_client_, nullptr);

  microsoft::routing_utils::proto::KVServerInfo serverInfo1;
  serverInfo1.set_server_name("AD_RETRIEVAL_KV_SERVER");

  auto adRetrival_async_client_ = connection_pool_.GetAsyncClient(serverInfo1);
  EXPECT_NE(adRetrival_async_client_.ok(), false);
  EXPECT_NE(*adRetrival_async_client_, nullptr);
}

}  // namespace privacy_sandbox::bidding_auction_servers
#endif  // CONNECTION_POOL_TEST_H_
