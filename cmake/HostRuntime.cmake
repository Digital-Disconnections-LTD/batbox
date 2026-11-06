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
add_library(batbox_roost src/cwd/Override.cpp
  src/shell/ShellBootstrap.cpp ${BATBOX_SHELL_BOOTSTRAP_SOURCE})
target_link_libraries(batbox_roost PUBLIC batbox_shell batbox_core batbox_event_loop)
target_compile_options(batbox_roost PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_cleanup src/cleanup/Roster.cpp src/cleanup/Retention.cpp
  src/cleanup/RetentionIndigenous.cpp)
target_link_libraries(batbox_cleanup PUBLIC batbox_core batbox_shell batbox_event_loop)
target_compile_options(batbox_cleanup PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_cleanup PRIVATE src/cleanup/FlightRetention.cpp
  src/cleanup/MiscRetention.cpp src/cleanup/ExtraRetentionIndigenous.cpp
  src/cleanup/Background.cpp src/cleanup/Terminal.cpp src/cleanup/TerminalIndigenous.cpp)
add_library(batbox_settings src/settings/RuleParser.cpp src/settings/RuleValidation.cpp
  src/settings/GadgetValidation.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_core ICU::uc)
target_compile_options(batbox_settings PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_settings PRIVATE src/settings/schema/Common.cpp
  src/settings/schema/Environment.cpp src/settings/schema/Permissions.cpp)
target_sources(batbox_settings PRIVATE src/settings/schema/Descriptor.cpp
  src/settings/schema/Main.cpp src/settings/schema/Runtime.cpp
  src/settings/schema/Scalar.cpp)
target_sources(batbox_settings PRIVATE src/settings/schema/IndigenousBindings.cpp
  src/settings/schema/imports/Bindings.cpp
  src/settings/schema/imports/HooksDescriptor.cpp src/settings/schema/imports/HooksParse.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_url)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_library(batbox_git_utils src/git/GitBasics.cpp src/git/IndigenousBasics.cpp
  src/git/GitBindings.cpp src/git/CanonicalRoot.cpp src/git/IndigenousCanonicalRoot.cpp)
target_link_libraries(batbox_git_utils PUBLIC batbox_git batbox_context batbox_process OpenSSL::Crypto ICU::uc)
target_compile_options(batbox_git_utils PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_settings PRIVATE src/settings/persistence/WatchTargets.cpp)
add_library(batbox_instruction_async
  src/instructions/project/async/IndigenousFs.cpp src/instructions/project/async/AsyncFile.cpp
  src/instructions/project/async/AsyncRules.cpp src/instructions/project/async/AsyncStash.cpp
  src/instructions/project/async/MemoryStages.cpp src/instructions/project/async/MemoryFinish.cpp
  src/instructions/project/async/ContextBinding.cpp src/instructions/project/async/ConditionalRules.cpp
  src/instructions/project/async/ManagedConditional.cpp src/instructions/project/async/NestedConditional.cpp
  src/instructions/hooks/InstructionsLoaded.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_instructions batbox_context batbox_event_loop)
target_compile_options(batbox_instruction_async PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_settings PRIVATE src/settings/watcher/IndigenousWatcher.cpp)
target_link_libraries(batbox_settings PUBLIC batbox_event_loop)
pkg_check_modules(PCRE2_16 REQUIRED IMPORTED_TARGET libpcre2-16)
add_library(batbox_glob src/glob/Regex16.cpp src/glob/IgnorePattern.cpp
  src/glob/IgnoreMatcher.cpp src/glob/PicomatchCore.cpp src/glob/PicomatchSyntax.cpp
  src/glob/PicomatchStars.cpp src/glob/PicomatchSafety.cpp src/glob/PicomatchMatch.cpp
  src/glob/DiscoveryMatcherBinding.cpp)
target_link_libraries(batbox_glob PUBLIC batbox_instructions PkgConfig::PCRE2_16)
target_compile_options(batbox_glob PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/OutsideRepl.cpp
  src/instructions/hooks/runtime/OutsideReplBranches.cpp
  src/instructions/hooks/runtime/HookAbort.cpp
  src/instructions/hooks/runtime/IndigenousCommandPrepared.cpp
  ${BATBOX_HOOK_COMMAND_SOURCE}
  src/instructions/hooks/runtime/HookMatching.cpp
  src/instructions/hooks/runtime/HookConfig.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_glob batbox_shell)
add_library(batbox_profiler src/profiler/Indigenous.cpp)
target_link_libraries(batbox_profiler PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_profiler PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_diagnostics src/diagnostics/DebugLog.cpp)
target_link_libraries(batbox_diagnostics PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_diagnostics PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE src/instructions/hooks/runtime/HookOutputParser.cpp)

add_library(batbox_flight_titles src/flights/Title.cpp src/flights/TitleText.cpp
  src/flights/TitlePoetry.cpp)
target_link_libraries(batbox_flight_titles PUBLIC batbox_query batbox_settings)
target_compile_options(batbox_flight_titles PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/BaseHookInput.cpp
  src/instructions/hooks/runtime/HttpHook.cpp src/instructions/hooks/runtime/SsrfGuard.cpp
  src/instructions/hooks/runtime/IndigenousHttp.cpp src/instructions/hooks/runtime/IfCondition.cpp
  src/instructions/hooks/runtime/HookSchema.cpp
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
target_sources(batbox_diagnostics PRIVATE src/diagnostics/DebugFilter.cpp)
add_library(batbox_auth)
target_link_libraries(batbox_auth PUBLIC batbox_core batbox_event_loop batbox_url ICU::uc)
target_compile_options(batbox_auth PRIVATE -Wall -Wextra -Wpedantic)


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
include(cmake/Monster.cmake)
include(cmake/Intl.cmake)
include(cmake/TextWidth.cmake)
include(cmake/Bootstrap.cmake)
include(cmake/AppState.cmake)
include(cmake/Cron.cmake)
include(cmake/TerminalEvents.cmake)
include(cmake/KeyboardInput.cmake)
include(cmake/Clipboard.cmake)
include(cmake/Truncate.cmake)

include(cmake/BatContext.cmake)
include(cmake/CronIntegration.cmake)
include(cmake/HeadlessCli.cmake)
include(cmake/StructuredRoot.cmake)
include(cmake/ProtocolHelpers.cmake)
include(cmake/ConversationRuntime.cmake)
include(cmake/StructuredFlight.cmake)
