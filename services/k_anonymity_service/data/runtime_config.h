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

#ifndef SERVICES_K_ANONYMITY_SERVICE_DATA_RUNTIME_CONFIG_H_
#define SERVICES_K_ANONYMITY_SERVICE_DATA_RUNTIME_CONFIG_H_

#include <string>

namespace microsoft::k_anonymity {

struct KAnonymityServiceRuntimeConfig {
  // Endpoint to replicate runtime data from if configured. This may be expanded
  // in the future to be multiple replication endpoints.
  std::string tee_replica_k_anonymity_host_addr = "";

  // Time in minutes to refresh data from a replica host if configured.
  int64_t replica_refresh_interval_min = 60;
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_K_ANONYMITY_SERVICE_DATA_RUNTIME_CONFIG_H_
