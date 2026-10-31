if(WIN32)
  set(BATBOX_SHELL_BOOTSTRAP_SOURCE src/shell/ShellBootstrapWin.cpp)
else()
  set(BATBOX_SHELL_BOOTSTRAP_SOURCE src/shell/ShellBootstrapIndigenous.cpp)
endif()
if(WIN32)
  set(BATBOX_HOOK_COMMAND_SOURCE src/instructions/hooks/runtime/IndigenousCommandProcessWin.cpp)
  set(BATBOX_HOOK_MANAGED_SOURCE src/instructions/hooks/runtime/IndigenousManagedHookProcessWin.cpp)
else()
  set(BATBOX_HOOK_COMMAND_SOURCE src/instructions/hooks/runtime/IndigenousCommandProcess.cpp)
  set(BATBOX_HOOK_MANAGED_SOURCE src/instructions/hooks/runtime/IndigenousManagedHookProcess.cpp)
endif()
add_library(batbox_roost src/cwd/State.cpp src/cwd/Override.cpp src/cwd/SetCwd.cpp
  src/shell/ShellBootstrap.cpp ${BATBOX_SHELL_BOOTSTRAP_SOURCE})
target_link_libraries(batbox_roost PUBLIC batbox_shell batbox_core batbox_event_loop)
target_compile_options(batbox_roost PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_cleanup src/cleanup/Roster.cpp src/cleanup/Retention.cpp
  src/cleanup/RetentionIndigenous.cpp src/cleanup/Shutdown.cpp)
target_link_libraries(batbox_cleanup PUBLIC batbox_core batbox_shell batbox_event_loop)
target_compile_options(batbox_cleanup PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_cleanup PRIVATE src/cleanup/FlightRetention.cpp
  src/cleanup/MiscRetention.cpp src/cleanup/ExtraRetentionIndigenous.cpp
  src/cleanup/Background.cpp src/cleanup/Terminal.cpp src/cleanup/TerminalIndigenous.cpp
  src/cleanup/Signals.cpp src/cleanup/SignalsIndigenousInterval.cpp
  src/cleanup/IndigenousSignalBridge.cpp src/cleanup/Exit.cpp src/cleanup/ExitIndigenous.cpp
  src/cleanup/StashThrottles.cpp)
add_library(batbox_settings src/settings/Stash.cpp src/settings/Sources.cpp
  src/settings/Paths.cpp src/settings/Merge.cpp src/settings/Parse.cpp
  src/settings/ParseBindings.cpp src/settings/Managed.cpp src/settings/LoaderSource.cpp
  src/settings/LoaderCascade.cpp src/settings/LoaderRaw.cpp src/settings/PermissionFilter.cpp
  src/settings/RuleParser.cpp src/settings/RuleValidation.cpp src/settings/GadgetValidation.cpp
  src/settings/ValidationFormat.cpp src/settings/Tips.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_core ICU::uc)
target_compile_options(batbox_settings PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_settings PRIVATE src/settings/schema/Common.cpp
  src/settings/schema/Environment.cpp src/settings/schema/Permissions.cpp
  src/settings/schema/McpEntry.cpp)
target_sources(batbox_settings PRIVATE src/settings/schema/Descriptor.cpp
  src/settings/schema/Main.cpp src/settings/schema/Runtime.cpp
  src/settings/schema/Scalar.cpp)
target_sources(batbox_settings PRIVATE src/settings/schema/IndigenousBindings.cpp
  src/settings/schema/imports/Bindings.cpp src/settings/schema/imports/Sandbox.cpp
  src/settings/schema/imports/HooksDescriptor.cpp src/settings/schema/imports/HooksParse.cpp
  src/settings/schema/imports/MarketplaceDescriptor.cpp
  src/settings/schema/imports/MarketplaceChecks.cpp src/settings/schema/imports/MarketplaceParse.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_url)
add_library(batbox_repository src/repository/Parse.cpp src/repository/Detector.cpp
  src/repository/Github.cpp)
target_link_libraries(batbox_repository PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_repository PRIVATE -Wall -Wextra -Wpedantic)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_library(batbox_git_utils src/git/GitBasics.cpp src/git/IndigenousBasics.cpp
  src/git/GitBindings.cpp src/git/CanonicalRoot.cpp src/git/IndigenousCanonicalRoot.cpp
  src/git/GitCommands.cpp src/git/BareRepository.cpp src/git/IndigenousBareRepository.cpp
  src/git/RemoteBase.cpp src/git/GitState.cpp src/git/RemoteIdentity.cpp src/git/Stash.cpp)
target_link_libraries(batbox_git_utils PUBLIC batbox_git batbox_context batbox_process OpenSSL::Crypto ICU::uc)
target_compile_options(batbox_git_utils PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_settings PRIVATE src/settings/persistence/InternalWrites.cpp
  src/settings/persistence/SafeJson.cpp src/settings/persistence/TrustedModes.cpp
  src/settings/persistence/Update.cpp src/settings/persistence/UpdateMerge.cpp
  src/settings/persistence/WatchTargets.cpp src/settings/persistence/ChangeEvents.cpp
  src/settings/persistence/MdmPoll.cpp src/settings/persistence/ApplyChange.cpp)
add_library(batbox_instruction_async
  src/instructions/project/async/IndigenousFs.cpp src/instructions/project/async/AsyncFile.cpp
  src/instructions/project/async/AsyncRules.cpp src/instructions/project/async/AsyncStash.cpp
  src/instructions/project/async/MemoryStages.cpp src/instructions/project/async/MemoryFinish.cpp
  src/instructions/project/async/ContextBinding.cpp src/instructions/project/async/ConditionalRules.cpp
  src/instructions/project/async/ManagedConditional.cpp src/instructions/project/async/NestedConditional.cpp
  src/instructions/project/async/ExternalWarning.cpp
  src/instructions/hooks/InstructionsLoaded.cpp src/instructions/hooks/InstructionHookBinding.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_instructions batbox_context batbox_event_loop)
target_compile_options(batbox_instruction_async PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_settings PRIVATE src/settings/watcher/IndigenousWatcher.cpp
  src/settings/watcher/DetectorLifecycle.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_event_loop)
pkg_check_modules(PCRE2_16 REQUIRED IMPORTED_TARGET libpcre2-16)
add_library(batbox_glob src/glob/Regex16.cpp src/glob/IgnorePattern.cpp
  src/glob/IgnoreMatcher.cpp src/glob/PicomatchCore.cpp src/glob/PicomatchSyntax.cpp
  src/glob/PicomatchStars.cpp src/glob/PicomatchSafety.cpp src/glob/PicomatchMatch.cpp
  src/glob/DiscoveryMatcherBinding.cpp)
target_link_libraries(batbox_glob PUBLIC batbox_instructions PkgConfig::PCRE2_16)
target_compile_options(batbox_glob PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_flight_activity src/flights/Activity.cpp src/flights/ActivityIndigenous.cpp)
target_link_libraries(batbox_flight_activity PUBLIC batbox_cleanup)
target_compile_options(batbox_flight_activity PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_quota src/quota/Quota.cpp)
target_link_libraries(batbox_quota PUBLIC batbox_core)
target_compile_options(batbox_quota PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_model_stash src/model_stash/DetectionState.cpp
  src/model_stash/DetectionCheck.cpp src/model_stash/DiffWriter.cpp
  src/model_stash/AsyncDetection.cpp src/model_stash/Date.cpp src/model_stash/ModelBindings.cpp)
target_link_libraries(batbox_model_stash PUBLIC batbox_query batbox_quota)
target_compile_options(batbox_model_stash PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_gitignore src/git/GitignoreUpdate.cpp src/git/GitignoreIndigenousIO.cpp)
target_link_libraries(batbox_gitignore PUBLIC batbox_git_utils)
target_compile_options(batbox_gitignore PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/OutsideRepl.cpp
  src/instructions/hooks/runtime/OutsideReplBranches.cpp
  src/instructions/hooks/runtime/HookAbort.cpp
  src/instructions/hooks/runtime/IndigenousCommand.cpp
  src/instructions/hooks/runtime/IndigenousCommandPrepared.cpp
  ${BATBOX_HOOK_COMMAND_SOURCE}
  src/instructions/hooks/runtime/HookMatching.cpp
  src/instructions/hooks/runtime/HookConfig.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_glob batbox_shell)
add_library(batbox_profiler src/profiler/Timeline.cpp src/profiler/QueryProfiler.cpp
  src/profiler/HeadlessProfiler.cpp src/profiler/Indigenous.cpp)
target_link_libraries(batbox_profiler PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_profiler PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_settings PRIVATE src/settings/mdm/Paths.cpp
  src/settings/mdm/RawRead.cpp src/settings/mdm/IndigenousRawRead.cpp
  src/settings/mdm/Parse.cpp src/settings/mdm/IndigenousParse.cpp
  src/settings/mdm/Store.cpp src/settings/mdm/AppBindings.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_process)
add_library(batbox_diagnostics src/diagnostics/DiagLogs.cpp)
target_link_libraries(batbox_diagnostics PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_diagnostics PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE src/instructions/hooks/runtime/HookOutputParser.cpp)

add_library(batbox_flight_titles src/flights/Title.cpp src/flights/TitleText.cpp
  src/flights/TitlePoetry.cpp src/flights/TitleBindings.cpp src/flights/TitleQueryBinding.cpp)
target_link_libraries(batbox_flight_titles PUBLIC batbox_query batbox_settings)
target_compile_options(batbox_flight_titles PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/BaseHookInput.cpp
  src/instructions/hooks/runtime/HttpHook.cpp src/instructions/hooks/runtime/SsrfGuard.cpp
  src/instructions/hooks/runtime/IndigenousHttp.cpp src/instructions/hooks/runtime/IfCondition.cpp
  src/instructions/hooks/runtime/HookSchema.cpp src/instructions/hooks/runtime/PermissionUpdate.cpp
  src/instructions/hooks/runtime/AsyncHookRoster.cpp
  src/instructions/hooks/runtime/AsyncHookRosterPoll.cpp
  src/instructions/hooks/runtime/AsyncHookRosterFinalize.cpp
  src/instructions/hooks/runtime/BackgroundHook.cpp
  ${BATBOX_HOOK_MANAGED_SOURCE}
  src/instructions/hooks/runtime/AsyncHookDetection.cpp
  src/instructions/hooks/runtime/HookPoetryStream.cpp
  src/instructions/hooks/runtime/IndigenousCommandPreparation.cpp
  src/instructions/hooks/runtime/IndigenousCommandFull.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_settings CURL::libcurl)
target_sources(batbox_diagnostics PRIVATE src/diagnostics/DebugFilter.cpp
  src/diagnostics/BufferedWriter.cpp src/diagnostics/DebugLog.cpp
  src/diagnostics/DebugLogIndigenous.cpp src/diagnostics/ErrorLogSink.cpp
  src/diagnostics/ErrorLogSinkIndigenous.cpp)
target_link_libraries(batbox_diagnostics PUBLIC batbox_cleanup)
target_sources(batbox_settings PRIVATE
  src/settings/remote/AppBindings.cpp
  src/settings/remote/Auth.cpp
  src/settings/remote/Checksum.cpp
  src/settings/remote/DefaultSecurityConfig.cpp
  src/settings/remote/Eligibility.cpp
  src/settings/remote/FetchAndLoad.cpp
  src/settings/remote/FetchOne.cpp
  src/settings/remote/LifecycleLoading.cpp
  src/settings/remote/LifecyclePolling.cpp
  src/settings/remote/IndigenousFile.cpp
  src/settings/remote/IndigenousHttp.cpp
  src/settings/remote/IndigenousSyncStash.cpp
  src/settings/remote/ResponseSchema.cpp
  src/settings/remote/Retry.cpp
  src/settings/remote/Security.cpp
  src/settings/remote/SyncStash.cpp
)
target_link_libraries(batbox_settings PUBLIC batbox_cleanup CURL::libcurl OpenSSL::Crypto)
add_library(batbox_auth
  src/auth/Provider.cpp
  src/auth/Credential.cpp
  src/auth/IndigenousCredentialIO.cpp
  src/auth/OAuthTokens.cpp
  src/auth/ApiKey.cpp
  src/auth/LegacyKey.cpp
  src/auth/OAuthScopes.cpp
  src/auth/helper/HelperSettings.cpp
  src/auth/helper/HelperStash.cpp
  src/auth/helper/HelperExecution.cpp
  src/auth/helper/IndigenousCommand.cpp
)
target_link_libraries(batbox_auth PUBLIC batbox_core batbox_event_loop batbox_url ICU::uc)
target_compile_options(batbox_auth PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_diagnostics PRIVATE src/diagnostics/ErrorLogQueue.cpp)

add_library(batbox_display src/display/Tags.cpp)
target_link_libraries(batbox_display PUBLIC batbox_glob)
target_compile_options(batbox_display PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_flights PRIVATE src/flights/DisplayTitle.cpp)
target_link_libraries(batbox_flights PUBLIC batbox_display)

include(cmake/SyntheticOutput.cmake)
include(cmake/HookRuntime.cmake)
include(cmake/AuthRuntime.cmake)
include(cmake/FilesystemRuntime.cmake)
include(cmake/HistoryRuntime.cmake)
include(cmake/ClientFactory.cmake)
include(cmake/Monster.cmake)
include(cmake/SdkEvents.cmake)
include(cmake/Intl.cmake)
include(cmake/TextWidth.cmake)
include(cmake/Bootstrap.cmake)
include(cmake/AppState.cmake)
include(cmake/Cron.cmake)
include(cmake/Mailbox.cmake)
include(cmake/TerminalEvents.cmake)
include(cmake/KeyboardInput.cmake)
include(cmake/Clipboard.cmake)
include(cmake/IndigenousYoga.cmake)
include(cmake/TerminalDom.cmake)
include(cmake/TerminalScreen.cmake)
include(cmake/KeyboardDispatch.cmake)
include(cmake/TerminalStyle.cmake)
include(cmake/TerminalOutput.cmake)
include(cmake/TerminalRenderer.cmake)
include(cmake/Truncate.cmake)

include(cmake/BatContext.cmake)
include(cmake/CronIntegration.cmake)
include(cmake/HeadlessCli.cmake)
include(cmake/TerminalRenderServices.cmake)
include(cmake/StructuredRoot.cmake)
include(cmake/ProtocolHelpers.cmake)
include(cmake/ConversationRuntime.cmake)
include(cmake/StructuredFlight.cmake)
include(cmake/RemoteTransports.cmake)
