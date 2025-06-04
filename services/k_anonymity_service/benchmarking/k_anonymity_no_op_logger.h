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

#ifndef SERVICES_COMMON_LOGGERS_K_ANONYMITY_NO_OP_LOGGER_H_
#define SERVICES_COMMON_LOGGERS_K_ANONYMITY_NO_OP_LOGGER_H_

#include <string_view>

#include "services/k_anonymity_service/benchmarking/k_anonymity_benchmarking_logger.h"

namespace microsoft::k_anonymity {

class KAnonymityNoOpLogger : public KAnonymityBenchmarkingLogger {
 public:
  KAnonymityNoOpLogger() : KAnonymityBenchmarkingLogger() {}

  void Begin() override {}
  void End() override {}

  void BuildInputBegin() override {}
  void BuildInputEnd() override {}
  void HandleResponseBegin() override {}
  void HandleResponseEnd() override {}
};

}  // namespace microsoft::k_anonymity

#endif  // SERVICES_COMMON_LOGGERS_K_ANONYMITY_NO_OP_LOGGER_H_
