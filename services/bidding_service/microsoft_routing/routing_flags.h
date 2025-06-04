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

#ifndef SERVICES_BIDDING_SERVICE_ROUTING_UTILS_FLAGS_H_
#define SERVICES_BIDDING_SERVICE_ROUTING_UTILS_FLAGS_H_

#include <optional>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"

namespace microsoft::routing_utils {

inline constexpr char SELECTION_KV_SERVER_ADDR[] = "SELECTION_KV_SERVER_ADDR";
inline constexpr char SELECTION_KV_SERVER_EGRESS_TLS[] =
    "SELECTION_KV_SERVER_EGRESS_TLS";
inline constexpr char SELECTION_KV_SERVER_TIMEOUT_MS[] =
    "SELECTION_KV_SERVER_TIMEOUT_MS";

inline constexpr absl::string_view kRoutingFlags[] = {
    SELECTION_KV_SERVER_ADDR, SELECTION_KV_SERVER_EGRESS_TLS,
    SELECTION_KV_SERVER_TIMEOUT_MS};

}  // namespace microsoft::routing_utils

#endif  // SERVICES_BIDDING_SERVICE_ROUTING_UTILS_FLAGS_H_
