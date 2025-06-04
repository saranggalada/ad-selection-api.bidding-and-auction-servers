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

#ifndef AD_SELECTION_SERVICES_KANON_SERVICE_RUNTIME_FLAGS_H_
#define AD_SELECTION_SERVICES_KANON_SERVICE_RUNTIME_FLAGS_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "services/common/constants/common_service_flags.h"

namespace microsoft::k_anonymity {

// Define runtime flag names.
inline constexpr absl::string_view PORT = "K_ANONYMITY_PORT";
inline constexpr absl::string_view HEALTHCHECK_PORT =
    "K_ANONYMITY_HEALTHCHECK_PORT";
inline constexpr absl::string_view ENABLE_K_ANONYMITY_SERVICE_BENCHMARK =
    "ENABLE_K_ANONYMITY_SERVICE_BENCHMARK";
inline constexpr absl::string_view TEE_REPLICA_K_ANONYMITY_HOST_ADDR =
    "TEE_REPLICA_K_ANONYMITY_HOST_ADDR";
inline constexpr absl::string_view REPLICA_REFRESH_INTERVAL_MIN =
    "REPLICA_REFRESH_INTERVAL_MIN";
inline constexpr absl::string_view
    KANON_TCMALLOC_BACKGROUND_RELEASE_RATE_BYTES_PER_SECOND =
        "KANON_TCMALLOC_BACKGROUND_RELEASE_RATE_BYTES_PER_SECOND";
inline constexpr absl::string_view KANON_TCMALLOC_MAX_TOTAL_THREAD_CACHE_BYTES =
    "KANON_TCMALLOC_MAX_TOTAL_THREAD_CACHE_BYTES";

inline constexpr int kNumRuntimeFlags = 7;
inline constexpr std::array<absl::string_view, kNumRuntimeFlags> kFlags = {
    PORT,
    HEALTHCHECK_PORT,
    ENABLE_K_ANONYMITY_SERVICE_BENCHMARK,
    TEE_REPLICA_K_ANONYMITY_HOST_ADDR,
    REPLICA_REFRESH_INTERVAL_MIN,
    KANON_TCMALLOC_BACKGROUND_RELEASE_RATE_BYTES_PER_SECOND,
    KANON_TCMALLOC_MAX_TOTAL_THREAD_CACHE_BYTES,
};

inline std::vector<absl::string_view> GetServiceFlags() {
  std::vector<absl::string_view> flags(kFlags.begin(),
                                       kFlags.begin() + kNumRuntimeFlags);

  for (absl::string_view flag :
       privacy_sandbox::bidding_auction_servers::kCommonServiceFlags) {
    flags.push_back(flag);
  }

  return flags;
}

}  // namespace microsoft::k_anonymity

#endif  // FLEDGE_SERVICES_BIDDING_SERVICE_RUNTIME_FLAGS_H_
