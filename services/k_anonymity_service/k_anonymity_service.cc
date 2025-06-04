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

#include "services/k_anonymity_service/k_anonymity_service.h"

#include <grpcpp/grpcpp.h>

#include "api/microsoft_k_anonymity.pb.h"
#include "services/common/metric/server_definition.h"
#include "src/telemetry/telemetry.h"

namespace microsoft::k_anonymity {
using privacy_sandbox::bidding_auction_servers::LogCommonMetric;

grpc::ServerUnaryReactor* KAnonymityService::Query(
    grpc::CallbackServerContext* context, const QueryRequest* request,
    QueryResponse* response) {
  LogCommonMetric(request, response);
  // Heap allocate the reactor. Deleted in reactor's OnDone call.
  auto* reactor = query_reactor_factory_(context, request, response,
                                         key_fetcher_manager_.get(),
                                         crypto_client_.get(), runtime_config_);
  reactor->Execute();
  return reactor;
}

grpc::ServerUnaryReactor* KAnonymityService::Join(
    grpc::CallbackServerContext* context, const JoinRequest* request,
    google::protobuf::Empty* response) {
  LogCommonMetric(request, response);
  // Heap allocate the reactor. Deleted in reactor's OnDone call.
  auto* reactor = join_reactor_factory_(context, request, response,
                                        key_fetcher_manager_.get(),
                                        crypto_client_.get(), runtime_config_);
  reactor->Execute();
  return reactor;
}

}  // namespace microsoft::k_anonymity
