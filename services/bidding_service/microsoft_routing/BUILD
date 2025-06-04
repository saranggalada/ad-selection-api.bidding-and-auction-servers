# Copyright (C) Microsoft Corporation. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "routing_flags",
    hdrs = [
        "routing_flags.h",
    ],
    visibility = [
        "//services/bidding_service:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "routing_utils",
    srcs = [
        "connection_pool.cc",
        "routing_utils.cc",
    ],
    hdrs = [
        "connection_pool.h",
        "routing_utils.h",
    ],
    visibility = [
        "//services/bidding_service:__subpackages__",
    ],
    deps = [
        ":routing_flags",
        "//api:microsoft_bidding_auction_servers_cc_grpc_proto",
        "//api:microsoft_bidding_auction_servers_cc_proto",
        "//services/bidding_service:runtime_flags",
        "//services/bidding_service/data:runtime_config",
        "//services/common/blob_fetch:blob_fetcher",
        "//services/common/clients/code_dispatcher:request_context",
        "//services/common/clients/config:config_client",
        "//services/common/clients/http:multi_curl_http_fetcher_async",
        "//services/common/clients/kv_server:kv_async_client",
        "//services/common/metric:server_definition",
        "//services/common/util:client_contexts",
        "//services/common/util:request_response_constants",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@google_privacysandbox_servers_common//src/logger:request_context_logger",
        "@google_privacysandbox_servers_common//src/roma/roma_service",
        "@google_privacysandbox_servers_common//src/util/status_macro:status_macros",
        "@google_privacysandbox_servers_common//src/util/status_macro:status_util",
        "@service_value_key_fledge_privacysandbox//public/query/v2:get_values_v2_cc_grpc",
        "@service_value_key_fledge_privacysandbox//public/query/v2:get_values_v2_cc_proto",
    ],
)

cc_test(
    name = "routing_utils_test",
    size = "small",
    srcs = ["routing_utils_test.cc"],
    flaky = True,
    deps = [
        ":routing_flags",
        ":routing_utils",
        "//services/bidding_service:runtime_flags",
        "//services/common/clients/kv_server:kv_async_client",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:reflection",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/roma/roma_service",
    ],
)

cc_test(
    name = "connection_pool_test",
    size = "small",
    srcs = ["connection_pool_test.cc"],
    flaky = True,
    deps = [
        ":routing_flags",
        ":routing_utils",
        "//services/bidding_service:runtime_flags",
        "//services/common/clients/kv_server:kv_async_client",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:reflection",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@google_privacysandbox_servers_common//src/roma/roma_service",
    ],
)
