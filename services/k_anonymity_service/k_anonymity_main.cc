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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/json_util.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "services/common/blob_fetch/blob_fetcher.h"
#include "services/common/clients/config/trusted_server_config_client.h"
#include "services/common/clients/config/trusted_server_config_client_util.h"
#include "services/common/encryption/crypto_client_factory.h"
#include "services/common/encryption/key_fetcher_factory.h"
#include "services/common/telemetry/configure_telemetry.h"
#include "services/common/util/file_util.h"
#include "services/common/util/request_response_constants.h"
#include "services/common/util/tcmalloc_utils.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_benchmarking_logger.h"
#include "services/k_anonymity_service/benchmarking/k_anonymity_no_op_logger.h"
#include "services/k_anonymity_service/data/runtime_config.h"
#include "services/k_anonymity_service/join_reactor.h"
#include "services/k_anonymity_service/k_anonymity_service.h"
#include "services/k_anonymity_service/runtime_flags.h"
#include "src/encryption/key_fetcher/key_fetcher_manager.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "src/util/rlimit_core_config.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::optional<uint16_t>, port, std::nullopt,
          "Port the server is listening on.");
ABSL_FLAG(std::optional<uint16_t>, healthcheck_port, std::nullopt,
          "Non-TLS port dedicated to healthchecks. Must differ from --port.");
ABSL_FLAG(std::optional<bool>, enable_k_anonymity_service_benchmark,
          std::nullopt, "Benchmark the K Anonymity service.");
ABSL_FLAG(
    bool, init_config_client, false,
    "Initialize config client to fetch any runtime flags not supplied from"
    " command line from cloud metadata store. False by default.");
ABSL_FLAG(std::optional<std::string>, tee_replica_k_anonymity_host_addr, "",
          "KAnon replica server address to use for replicating state in a "
          "cluster configuration.");
ABSL_FLAG(std::optional<int64_t>, replica_refresh_interval_min, 60,
          "The time in minutes to wait between replica data refreshes");
ABSL_FLAG(std::optional<int64_t>,
          kanon_tcmalloc_background_release_rate_bytes_per_second, std::nullopt,
          "Amount of cached memory in bytes that is returned back to the "
          "system per second");
ABSL_FLAG(std::optional<int64_t>, kanon_tcmalloc_max_total_thread_cache_bytes,
          std::nullopt,
          "Maximum amount of cached memory in bytes across all threads (or "
          "logical CPUs)");

namespace microsoft::k_anonymity {

using ::google::scp::cpio::BlobStorageClientFactory;
using ::google::scp::cpio::BlobStorageClientInterface;
using ::google::scp::cpio::Cpio;
using ::google::scp::cpio::CpioOptions;
using ::google::scp::cpio::LogOption;
using ::grpc::Server;
using ::grpc::ServerBuilder;
using privacy_sandbox::bidding_auction_servers::CreateCryptoClient;
using privacy_sandbox::bidding_auction_servers::InitTelemetry;
using privacy_sandbox::bidding_auction_servers::MaySetBackgroundReleaseRate;
using privacy_sandbox::bidding_auction_servers::MaySetMaxTotalThreadCacheBytes;
using privacy_sandbox::bidding_auction_servers::TrustedServerConfigUtil;
using privacy_sandbox::bidding_auction_servers::TrustedServersConfigClient;

absl::StatusOr<TrustedServersConfigClient> GetConfigClient(
    absl::string_view config_param_prefix) {
  TrustedServersConfigClient config_client(GetServiceFlags());

  // Based service flags for port and benchmarking configuration.
  config_client.SetFlag(FLAGS_port, PORT);
  config_client.SetFlag(FLAGS_healthcheck_port, HEALTHCHECK_PORT);
  config_client.SetFlag(FLAGS_enable_k_anonymity_service_benchmark,
                        ENABLE_K_ANONYMITY_SERVICE_BENCHMARK);

  // Shared B&A KMS and test flags.
  config_client.SetFlag(FLAGS_test_mode,
                        privacy_sandbox::bidding_auction_servers::TEST_MODE);
  config_client.SetFlag(
      FLAGS_public_key_endpoint,
      privacy_sandbox::bidding_auction_servers::PUBLIC_KEY_ENDPOINT);
  config_client.SetFlag(FLAGS_primary_coordinator_private_key_endpoint,
                        privacy_sandbox::bidding_auction_servers::
                            PRIMARY_COORDINATOR_PRIVATE_KEY_ENDPOINT);
  config_client.SetFlag(FLAGS_secondary_coordinator_private_key_endpoint,
                        privacy_sandbox::bidding_auction_servers::
                            SECONDARY_COORDINATOR_PRIVATE_KEY_ENDPOINT);
  config_client.SetFlag(FLAGS_primary_coordinator_account_identity,
                        privacy_sandbox::bidding_auction_servers::
                            PRIMARY_COORDINATOR_ACCOUNT_IDENTITY);
  config_client.SetFlag(FLAGS_secondary_coordinator_account_identity,
                        privacy_sandbox::bidding_auction_servers::
                            SECONDARY_COORDINATOR_ACCOUNT_IDENTITY);
  config_client.SetFlag(FLAGS_gcp_primary_workload_identity_pool_provider,
                        privacy_sandbox::bidding_auction_servers::
                            GCP_PRIMARY_WORKLOAD_IDENTITY_POOL_PROVIDER);
  config_client.SetFlag(FLAGS_gcp_secondary_workload_identity_pool_provider,
                        privacy_sandbox::bidding_auction_servers::
                            GCP_SECONDARY_WORKLOAD_IDENTITY_POOL_PROVIDER);
  config_client.SetFlag(FLAGS_gcp_primary_key_service_cloud_function_url,
                        privacy_sandbox::bidding_auction_servers::
                            GCP_PRIMARY_KEY_SERVICE_CLOUD_FUNCTION_URL);
  config_client.SetFlag(FLAGS_gcp_secondary_key_service_cloud_function_url,
                        privacy_sandbox::bidding_auction_servers::
                            GCP_SECONDARY_KEY_SERVICE_CLOUD_FUNCTION_URL);

  config_client.SetFlag(
      FLAGS_primary_coordinator_region,
      privacy_sandbox::bidding_auction_servers::PRIMARY_COORDINATOR_REGION);
  config_client.SetFlag(
      FLAGS_secondary_coordinator_region,
      privacy_sandbox::bidding_auction_servers::SECONDARY_COORDINATOR_REGION);
  config_client.SetFlag(
      FLAGS_private_key_cache_ttl_seconds,
      privacy_sandbox::bidding_auction_servers::PRIVATE_KEY_CACHE_TTL_SECONDS);
  config_client.SetFlag(FLAGS_key_refresh_flow_run_frequency_seconds,
                        privacy_sandbox::bidding_auction_servers::
                            KEY_REFRESH_FLOW_RUN_FREQUENCY_SECONDS);

  // Shared B&A service debug and telem flags.
  config_client.SetFlag(
      FLAGS_telemetry_config,
      privacy_sandbox::bidding_auction_servers::TELEMETRY_CONFIG);
  config_client.SetFlag(
      FLAGS_consented_debug_token,
      privacy_sandbox::bidding_auction_servers::CONSENTED_DEBUG_TOKEN);
  config_client.SetFlag(
      FLAGS_enable_otel_based_logging,
      privacy_sandbox::bidding_auction_servers::ENABLE_OTEL_BASED_LOGGING);
  config_client.SetFlag(FLAGS_ps_verbosity,
                        privacy_sandbox::bidding_auction_servers::PS_VERBOSITY);

  // Flags to control replication, if configured.
  config_client.SetFlag(FLAGS_tee_replica_k_anonymity_host_addr,
                        TEE_REPLICA_K_ANONYMITY_HOST_ADDR);
  config_client.SetFlag(FLAGS_replica_refresh_interval_min,
                        REPLICA_REFRESH_INTERVAL_MIN);

  if (absl::GetFlag(FLAGS_init_config_client)) {
    PS_RETURN_IF_ERROR(config_client.Init(config_param_prefix)).LogError()
        << "Config client failed to initialize.";
  }
  // Set verbosity and resource limits
  privacy_sandbox::server_common::log::SetGlobalPSVLogLevel(
      config_client.GetIntParameter(
          privacy_sandbox::bidding_auction_servers::PS_VERBOSITY));
  config_client.SetFlag(
      FLAGS_kanon_tcmalloc_background_release_rate_bytes_per_second,
      KANON_TCMALLOC_BACKGROUND_RELEASE_RATE_BYTES_PER_SECOND);
  config_client.SetFlag(FLAGS_kanon_tcmalloc_max_total_thread_cache_bytes,
                        KANON_TCMALLOC_MAX_TOTAL_THREAD_CACHE_BYTES);

  PS_LOG(INFO) << "Successfully constructed the config client.";
  return config_client;
}

absl::string_view GetStringParameterSafe(
    const TrustedServersConfigClient& client, absl::string_view name) {
  if (client.HasParameter(name)) {
    return client.GetStringParameter(name);
  }
  return "";
}

// Brings up the gRPC KAnonymityService on FLAGS_port.
absl::Status RunServer() {
  TrustedServerConfigUtil config_util(absl::GetFlag(FLAGS_init_config_client));
  PS_ASSIGN_OR_RETURN(TrustedServersConfigClient config_client,
                      GetConfigClient(config_util.GetConfigParameterPrefix()));

  MaySetBackgroundReleaseRate(config_client.GetInt64Parameter(
      KANON_TCMALLOC_BACKGROUND_RELEASE_RATE_BYTES_PER_SECOND));
  MaySetMaxTotalThreadCacheBytes(config_client.GetInt64Parameter(
      KANON_TCMALLOC_MAX_TOTAL_THREAD_CACHE_BYTES));
  std::string_view port = config_client.GetStringParameter(PORT);
  std::string server_address = absl::StrCat("0.0.0.0:", port);

  privacy_sandbox::server_common::GrpcInit gprc_init;

  bool enable_k_anonymity_service_benchmark =
      config_client.GetBooleanParameter(ENABLE_K_ANONYMITY_SERVICE_BENCHMARK);

  InitTelemetry<QueryRequest>(
      config_util, config_client,
      privacy_sandbox::bidding_auction_servers::metric::kBs);
  InitTelemetry<JoinRequest>(
      config_util, config_client,
      privacy_sandbox::bidding_auction_servers::metric::kBs);

  KAnonymityServiceRuntimeConfig runtime_config = {
      .tee_replica_k_anonymity_host_addr =
          config_client.GetStringParameter(TEE_REPLICA_K_ANONYMITY_HOST_ADDR)
              .data(),
      .replica_refresh_interval_min =
          config_client.GetInt64Parameter(REPLICA_REFRESH_INTERVAL_MIN)};

  auto query_reactor_factory =
      [enable_k_anonymity_service_benchmark](
          grpc::CallbackServerContext* context, const QueryRequest* request,
          QueryResponse* response,
          KeyFetcherManagerInterface* key_fetcher_manager,
          CryptoClientWrapperInterface* crypto_client,
          const KAnonymityServiceRuntimeConfig& runtime_config) {
        std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarkingLogger;
        if (enable_k_anonymity_service_benchmark) {
          benchmarkingLogger = std::make_unique<KAnonymityBenchmarkingLogger>(
              FormatTime(absl::Now()));
        } else {
          benchmarkingLogger = std::make_unique<KAnonymityNoOpLogger>();
        }
        auto query_reactor = std::make_unique<QueryReactor>(
            context, request, response, std::move(benchmarkingLogger),
            key_fetcher_manager, crypto_client, runtime_config);
        return query_reactor.release();
      };

  auto join_reactor_factory =
      [enable_k_anonymity_service_benchmark](
          grpc::CallbackServerContext* context, const JoinRequest* request,
          const google::protobuf::Empty* response,
          KeyFetcherManagerInterface* key_fetcher_manager,
          CryptoClientWrapperInterface* crypto_client,
          const KAnonymityServiceRuntimeConfig& runtime_config) {
        std::unique_ptr<KAnonymityBenchmarkingLogger> benchmarkingLogger;
        if (enable_k_anonymity_service_benchmark) {
          benchmarkingLogger = std::make_unique<KAnonymityBenchmarkingLogger>(
              FormatTime(absl::Now()));
        } else {
          benchmarkingLogger = std::make_unique<KAnonymityNoOpLogger>();
        }
        auto join_reactor = std::make_unique<JoinReactor>(
            context, request, response, std::move(benchmarkingLogger),
            key_fetcher_manager, crypto_client, runtime_config);
        return join_reactor.release();
      };

  KAnonymityService k_anonymity_service(
      std::move(query_reactor_factory), std::move(join_reactor_factory),
      CreateKeyFetcherManager(config_client, /*public_key_fetcher=*/nullptr),
      CreateCryptoClient(), std::move(runtime_config));

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  // This server is expected to accept insecure connections as it will be
  // deployed behind an HTTPS load balancer that terminates TLS.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  if (config_client.HasParameter(HEALTHCHECK_PORT)) {
    CHECK(config_client.GetStringParameter(HEALTHCHECK_PORT) !=
          config_client.GetStringParameter(PORT))
        << "Healthcheck port must be unique.";
    builder.AddListeningPort(
        absl::StrCat("0.0.0.0:",
                     config_client.GetStringParameter(HEALTHCHECK_PORT)),
        grpc::InsecureServerCredentials());
  }

  // Set max message size to 256 MB.
  builder.AddChannelArgument(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH,
                             256L * 1024L * 1024L);
  builder.RegisterService(&k_anonymity_service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  if (server == nullptr) {
    return absl::UnavailableError("Error starting Server.");
  }
  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  PS_LOG(INFO) << "Server listening on " << server_address;
  server->Wait();
  return absl::OkStatus();
}
}  // namespace microsoft::k_anonymity

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);
  privacysandbox::server_common::SetRLimits({
      .enable_core_dumps = PS_ENABLE_CORE_DUMPS,
  });
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  google::scp::cpio::CpioOptions cpio_options;

  bool init_config_client = absl::GetFlag(FLAGS_init_config_client);
  if (init_config_client) {
    cpio_options.log_option = google::scp::cpio::LogOption::kConsoleLog;
    CHECK(google::scp::cpio::Cpio::InitCpio(cpio_options).Successful())
        << "Failed to initialize CPIO library";
  }

  CHECK_OK(microsoft::k_anonymity::RunServer()) << "Failed to run server.";

  if (init_config_client) {
    google::scp::cpio::Cpio::ShutdownCpio(cpio_options);
  }

  return 0;
}
