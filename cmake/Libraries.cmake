include(cmake/Icu.cmake)
add_library(batbox_core
  src/runtime/Version.cpp
  src/runtime/ArrayJoin.cpp
  src/runtime/JsonValue.cpp
  src/runtime/Platform.cpp
  src/runtime/Strings.cpp
  src/runtime/JsonParse.cpp
  src/runtime/Encoding.cpp
  src/runtime/EncodeUtf8.cpp
  src/runtime/PrettyJson.cpp
  src/runtime/PathSanitization.cpp src/utils/hash.cpp
  src/runtime/Unicode.cpp
  src/shell/PosixPath.cpp
  src/utils/envUtils.cpp
  src/utils/tokens.cpp
  src/utils/messages.cpp
)
# The one place the project version enters the sources.
set_source_files_properties(src/runtime/Version.cpp PROPERTIES
  COMPILE_DEFINITIONS "BATBOX_VERSION=\"${PROJECT_VERSION}\"")
target_include_directories(batbox_core PUBLIC src)
target_link_libraries(batbox_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads ICU::uc)
target_compile_options(batbox_core PRIVATE -Wall -Wextra -Wpedantic)
# The conversation/wire protocol mechanism layer: message construction and
# pairing, request assembly, stream reduction, retry policy, API error
# rendering, content normalization, model naming, and formatting. It was
# compiled into batbox_core, mixing L2 protocol code with the foundation.
# batbox_core links this target PUBLIC so existing consumers keep resolving.
add_library(batbox_protocol
  src/messages/TextContent.cpp
  src/messages/Construction.cpp
  src/messages/Context.cpp
  src/messages/Normalization.cpp
  src/messages/Pairing.cpp
  src/requests/assembly.cpp
  src/requests/params.cpp
  src/requests/RequestBuilder.cpp
  src/requests/PoetryStash.cpp
  src/streaming/Reducer.cpp
  src/retry/Machine.cpp src/retry/MachineDirect.cpp
  src/retry/ErrorDetails.cpp
  src/retry/Policy.cpp
  src/retry/Retry.cpp
  src/retry/Sleep.cpp
  src/api_errors/Messages.cpp
  src/api_errors/Helpers.cpp
  src/api_errors/Formatting.cpp
  src/api_errors/RendererHelpers.cpp
  src/api_errors/Renderer.cpp
  src/normalization/AttachmentDeltas.cpp
  src/normalization/AttachmentMore.cpp
  src/normalization/AttachmentPlanText.cpp
  src/normalization/AttachmentPlans.cpp
  src/normalization/AttachmentGadgets.cpp
  src/normalization/Attachments.cpp
  src/normalization/Cleanup.cpp
  src/normalization/Content.cpp
  src/normalization/ImageValidation.cpp
  src/normalization/Pipeline.cpp
  src/normalization/References.cpp
  src/model_config/ModelNames.cpp
  src/formatting/Format.cpp
  # Tag-string scrubbing: message normalization living with the other
  # message transforms, reached through the core aggregate.
  src/messages/Tags.cpp
  # services/ helpers: token estimation, API usage, compaction grouping --
  # protocol-level helpers over messages, moved with the layer.
  src/services/tokenEstimation.cpp
  src/services/api/emptyUsage.cpp
  src/services/api/batbox.cpp
  src/services/compact/autoCompact.cpp
  src/services/compact/grouping.cpp
)
target_include_directories(batbox_protocol PUBLIC src)
target_link_libraries(batbox_protocol PUBLIC batbox_core batbox_glob)
target_compile_options(batbox_protocol PRIVATE -Wall -Wextra -Wpedantic)
# Same aggregate pattern as batbox_file_gadgets: no core TU references
# protocol symbols, so the core -> protocol archive order is acyclic at the
# declaration level.
target_link_libraries(batbox_core PUBLIC batbox_protocol)
include(cmake/FileGadgets.cmake)
include(cmake/EventLoop.cmake)
add_library(batbox_connectors src/connectors/Searxng.cpp src/connectors/Scrapling.cpp
  src/connectors/Executable.cpp)
target_include_directories(batbox_connectors PUBLIC src)
target_link_libraries(batbox_connectors PUBLIC batbox_core CURL::libcurl)
target_compile_options(batbox_connectors PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_transport
  src/transport/Messages.cpp
  src/transport/Errors.cpp
  src/transport/SSE.cpp src/transport/Client.cpp)
target_sources(batbox_transport PRIVATE src/transport/RetryAdapter.cpp
  src/transport/RawHttp.cpp src/transport/Perform.cpp src/transport/WireClient.cpp
  src/transport/openai_chat/Translate.cpp src/transport/openai_chat/StreamDecoder.cpp
  src/transport/openai_chat/OpenAIChatClient.cpp)
target_include_directories(batbox_transport PUBLIC src)
target_link_libraries(batbox_transport PUBLIC batbox_core CURL::libcurl)
target_compile_options(batbox_transport PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_core PRIVATE
  src/accounting/Cost.cpp
  src/accounting/Flight.cpp
)
# The gadget mechanism layer: the gadget interface, schema, input
# normalization, catalog, and the app-state execution modifiers. Like the
# protocol and file-gadget layers, it was compiled into batbox_core; it is
# L2 execution/mechanism code, not foundation. batbox_core re-exposes it
# PUBLIC so existing consumers keep resolving.
add_library(batbox_gadget_execution
  src/gadgets/Gadget.cpp
  src/gadgets/Schema.cpp
  src/gadgets/InputNormalization.cpp
  src/gadgets/Catalog.cpp
  src/execution/AppStateModifier.cpp
  src/execution/TypedAppStateModifier.cpp
)
target_include_directories(batbox_gadget_execution PUBLIC src)
target_link_libraries(batbox_gadget_execution PUBLIC batbox_core
  batbox_protocol batbox_file_gadgets batbox_app_state batbox_gadget_results)
target_compile_options(batbox_gadget_execution PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_core PUBLIC batbox_gadget_execution)
add_library(batbox_model
  src/model/SimpleQueries.cpp
  src/model/StreamObservation.cpp
  src/model/Completion.cpp
  src/model/PostStream.cpp
  src/model/Lifecycle.cpp
  src/model/LiveStream.cpp
  src/model/LiveStreamAdapter.cpp
  src/model/ModelWrappers.cpp
  src/model/NonStreaming.cpp
  src/model/Preflight.cpp
  src/model/QueryModel.cpp
  src/model/StreamingAttempt.cpp
  src/model/Watchdog.cpp
)
target_link_libraries(batbox_model PUBLIC batbox_transport batbox_core batbox_event_loop)
target_compile_options(batbox_model PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_compaction
  src/compaction/Restore.cpp
  src/compaction/Summary.cpp
  src/compaction/Workflow.cpp
  src/compaction/Auto.cpp src/compaction/FlightPolicy.cpp
  src/compaction/Helpers.cpp src/compaction/Cleanup.cpp
  src/compaction/Poetry.cpp src/compaction/poetryEntries/Base.cpp
  src/compaction/poetryEntries/Boundary.cpp)
target_link_libraries(batbox_compaction PUBLIC batbox_core)
target_compile_options(batbox_compaction PRIVATE -Wall -Wextra -Wpedantic)
if(WIN32)
  set(BATBOX_SHELL_SPAWN_SOURCE src/shell/SpawnWin.cpp)
  set(BATBOX_SHELL_SNAPSHOT_SOURCE src/shell/SnapshotRunWin.cpp)
else()
  set(BATBOX_SHELL_SPAWN_SOURCE src/shell/Spawn.cpp)
  set(BATBOX_SHELL_SNAPSHOT_SOURCE src/shell/SnapshotRun.cpp)
endif()
add_library(batbox_shell
  src/shell/BashProvider.cpp src/shell/SnapshotIntegrations.cpp
  src/shell/SnapshotUser.cpp src/shell/SnapshotScript.cpp ${BATBOX_SHELL_SNAPSHOT_SOURCE}
  src/shell/ShellQuoteChecks.cpp src/shell/PipeCommand.cpp
  src/shell/CommandQuoting.cpp src/shell/CommandQuotingSyntax.cpp
  src/shell/CommandRedirects.cpp src/shell/CommandPrefix.cpp
  src/shell/Heredoc.cpp src/shell/HeredocCollect.cpp src/shell/HeredocSyntax.cpp
  src/shell/ShellQuoteParse.cpp src/shell/ShellQuoteQuote.cpp
  src/shell/CommandSplitter.cpp src/shell/CommandSplitterRedirections.cpp
  # The mission roster, output and stop gadgets now compile in batbox_missions.
  src/shell/ManagedShell.cpp ${BATBOX_SHELL_SPAWN_SOURCE} src/shell/WindowsPaths.cpp
  src/shell/Output.cpp src/shell/ShellDiscovery.cpp
  src/shell/CommandHandle.cpp src/shell/TailFile.cpp src/shell/MissionOutputPoller.cpp
  src/shell/FormatDuration.cpp src/shell/ShellCwdCompletion.cpp
  src/gadgets/bash/Helpers.cpp src/gadgets/bash/Result.cpp src/gadgets/bash/BashGadget.cpp
  src/gadgets/bash/Schema.cpp src/gadgets/bash/Accumulator.cpp
  src/gadgets/bash/Hints.cpp src/gadgets/bash/CwdPolicy.cpp)
target_sources(batbox_shell PRIVATE src/gadgets/bash/RunShellCommand.cpp)
target_sources(batbox_shell PRIVATE src/shell/AsyncCommandResult.cpp
  src/shell/LoopMissionOutput.cpp)
target_sources(batbox_shell PRIVATE
  src/gadgets/bash/AsyncRunShellCommand.cpp src/gadgets/bash/AsyncRunBindings.cpp
  src/gadgets/bash/AsyncShellBinding.cpp src/gadgets/bash/BashCompletion.cpp
  src/gadgets/bash/AsyncBashCall.cpp src/gadgets/bash/AsyncBashFactory.cpp)
# batbox_missions includes shell headers (ManagedShell, MissionOutputPoller);
# the shell consumes the roster -- the declared cycle folds in the aggregate.
target_link_libraries(batbox_shell PUBLIC batbox_core batbox_event_loop batbox_missions)
target_compile_options(batbox_shell PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_query
  src/execution/Dispatch.cpp src/execution/Call.cpp
  src/execution/Orchestration.cpp
  src/query/Recovery.cpp
  src/query/Entry.cpp src/query/Setup.cpp src/query/ModelPass.cpp
  src/query/Completion.cpp src/query/GadgetTurn.cpp src/query/Engine.cpp
  src/query/StopHooks.cpp src/query/StopHooksPartner.cpp)
target_link_libraries(batbox_query PUBLIC batbox_model batbox_core)
target_compile_options(batbox_query PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_query PRIVATE src/query/AsyncSetupStart.cpp src/query/AsyncSetupFinish.cpp)
target_sources(batbox_query PRIVATE
  src/query/AsyncModelStart.cpp src/query/AsyncModelMessage.cpp
  src/query/AsyncModelRecovery.cpp
  src/query/AsyncEngineStart.cpp src/query/AsyncEngineModel.cpp src/query/AsyncEngineAbort.cpp)
target_link_libraries(batbox_query PUBLIC batbox_event_loop)
add_library(batbox_bats
  src/bats/RunBatPreflight.cpp
  src/bats/RunBatHelpers.cpp
  src/bats/RunBatSetup.cpp src/bats/RunBatLoop.cpp
  src/bats/ResumeBat.cpp src/bats/BatGadgetResults.cpp src/bats/AsyncLifecycle.cpp
  src/bats/BatGadgets.cpp src/bats/BatMissionProgress.cpp src/bats/BatMissionState.cpp
  src/bats/BatMissionNotification.cpp src/bats/BatMissionBinding.cpp
  src/bats/BatMissionRegistration.cpp src/bats/BatMissionRuntime.cpp
  src/bats/BatMissionForeground.cpp
  src/bats/BatGadgetPresentation.cpp
  src/bats/BatGadgetPreflight.cpp
  src/bats/BatGadgetWorktree.cpp
  src/bats/StashSafe.cpp src/bats/SubagentContext.cpp
  src/bats/SnapshotBridge.cpp
  src/bats/TypedBatState.cpp)
target_link_libraries(batbox_bats PUBLIC batbox_query)
target_compile_options(batbox_bats PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/BatGadgets.cmake)
include(cmake/FlightStorage.cmake)
include(cmake/GadgetResults.cmake)
include(cmake/WebGadgets.cmake)
include(cmake/MediaGadgets.cmake)
include(cmake/McpGadgets.cmake)
include(cmake/InstructionGadgets.cmake)
include(cmake/Foundation.cmake)
include(cmake/MemoryPrefetch.cmake)
include(cmake/IndigenousDependencies.cmake)
include(cmake/HostRuntime.cmake)
include(cmake/FtxuiDependency.cmake)
include(cmake/FtxuiPresentation.cmake)
include(cmake/UserInput.cmake)
include(cmake/AttachmentTriggers.cmake)
include(cmake/Attachments.cmake)
include(cmake/CoreCommands.cmake)

# Scoped context is shared by low-level timer/future adapters and the owner
# loop: it belongs to the event-loop mechanism, not the foundation bundle.
target_sources(batbox_event_loop PRIVATE src/event_loop/AsyncContext.cpp)
include(cmake/CloudProviders.cmake)
