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

#include <gmock/gmock.h>

#include <google/protobuf/util/json_util.h>

#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "services/bidding_service/runtime_flags.h"
#include "services/common/clients/code_dispatcher/request_context.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace microsoft::routing_utils {

namespace {

class MockRoutingUtils : public RoutingUtils {
 public:
  explicit MockRoutingUtils(const TrustedServersConfigClient& config_client)
      : RoutingUtils(config_client, nullptr) {}

  MOCK_METHOD(void, SendRequestToKVServer,
              (const proto::KVGetValuesRequest& request,
               proto::BatchKVGetValuesResponse& batch_response,
               std::atomic<int>& batch_response_counter),
              (override));
};

class RoutingUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override { config_.SetFlagForTest(kTrue, TEST_MODE); }

  void TearDown() override {}

  TrustedServersConfigClient config_{{}};
};

TEST_F(RoutingUtilsTest, ProcessRequestTest) {
  TrustedServersConfigClient config_client(GetServiceFlags());
  config_client.SetFlagForTest("127.0.0.1:50073",
                               std::string(SELECTION_KV_SERVER_ADDR));
  config_client.SetFlagForTest(kTrue,
                               std::string(SELECTION_KV_SERVER_EGRESS_TLS));
  config_client.SetFlagForTest("200",
                               std::string(SELECTION_KV_SERVER_TIMEOUT_MS));

  MockRoutingUtils mock_routing_utils(config_client);

  google::scp::roma::proto::FunctionBindingIoProto input_output_proto;
  google::scp::roma::FunctionBindingPayload<RomaRequestSharedContext>
      mock_request{input_output_proto, {}};

  mock_request.io_proto.set_input_string(
      "{\"requests\": [{\"server_info\": {\"server_name\": "
      "\"test_server\"}}]}");

  EXPECT_CALL(mock_routing_utils, SendRequestToKVServer(_, _, _))
      .WillOnce(Invoke([&](const proto::KVGetValuesRequest& request,
                           proto::BatchKVGetValuesResponse& batch_response,
                           std::atomic<int>& batch_response_counter) {
        PS_LOG(INFO) << "Mock SendRequestToKVServer: \n"
                     << request.DebugString();

        // Set up the expected response
        auto* single_partition = new kv_server::v2::ResponsePartition();
        single_partition->set_id(1);
        single_partition->set_string_output("Test response");

        auto* get_values_response = new kv_server::v2::GetValuesResponse();
        get_values_response->set_allocated_single_partition(single_partition);

        auto* kv_response = batch_response.add_responses();
        kv_response->set_allocated_get_values_response(get_values_response);
        kv_response->set_server_name(request.server_info().server_name());

        batch_response_counter.fetch_add(1);

        PS_LOG(INFO) << "Mock BatchKVGetValuesResponse: \n"
                     << batch_response.DebugString();
      }));

  mock_routing_utils.FetchAdditionalSignals(mock_request);

  proto::BatchKVGetValuesResponse batch_response;
  std::string json_string = mock_request.io_proto.output_string();
  EXPECT_TRUE(!json_string.empty());

  auto conv_status =
      google::protobuf::util::JsonStringToMessage(json_string, &batch_response);

  EXPECT_EQ(batch_response.responses_size(), 1);

  auto actual_response = batch_response.responses(0);

  EXPECT_EQ(actual_response.server_name(), "test_server");
  EXPECT_EQ(actual_response.get_values_response().single_partition().id(), 1);
  EXPECT_EQ(
      actual_response.get_values_response().single_partition().string_output(),
      "Test response");
}

}  // namespace
}  // namespace microsoft::routing_utils
