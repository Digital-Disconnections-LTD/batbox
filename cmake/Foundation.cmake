find_package(ICU REQUIRED COMPONENTS uc i18n)
include(cmake/CmarkDependency.cmake)
target_sources(batbox_instructions PRIVATE
  src/instructions/project/MarkdownLexer.cpp src/instructions/project/ProjectComments.cpp
  src/instructions/project/ProjectContent.cpp src/instructions/project/ProjectFrontmatter.cpp
  src/instructions/project/ProjectIncludes.cpp src/instructions/project/ProjectPaths.cpp
  src/instructions/project/ProjectContext.cpp)
target_link_libraries(batbox_instructions PUBLIC PkgConfig::CMARK_GFM ICU::i18n)
add_library(batbox_context src/context/ContextDate.cpp src/context/ContextSettings.cpp
  src/context/AsyncContextStash.cpp src/context/AsyncGitStatus.cpp
  src/context/AsyncSystemContext.cpp src/context/AsyncUserContext.cpp)
target_link_libraries(batbox_context PUBLIC batbox_instructions batbox_event_loop)
target_compile_options(batbox_context PRIVATE -Wall -Wextra -Wpedantic)
# The default system prompt is poetry/system.md, embedded as bytes so the
# binary never reads it at run time.
set(BATBOX_SYSTEM_POETRY_SOURCE ${CMAKE_SOURCE_DIR}/poetry/system.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BATBOX_SYSTEM_POETRY_SOURCE})
file(READ ${BATBOX_SYSTEM_POETRY_SOURCE} BATBOX_SYSTEM_POETRY_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BATBOX_SYSTEM_POETRY_BYTES "${BATBOX_SYSTEM_POETRY_HEX}")
string(REGEX REPLACE "((0x..,){24})" "\\1\n" BATBOX_SYSTEM_POETRY_BYTES "${BATBOX_SYSTEM_POETRY_BYTES}")
configure_file(src/poetryEntries/SystemPoetryDefault.cpp.in
  ${CMAKE_BINARY_DIR}/generated/poetryEntries/SystemPoetryDefault.cpp @ONLY)
add_library(batbox_poetryEntries
  src/poetryEntries/BatPoetry.cpp src/poetryEntries/EnvironmentBasics.cpp src/poetryEntries/EnvironmentRender.cpp
  src/poetryEntries/GuidanceCore.cpp src/poetryEntries/GuidanceFlight.cpp
  src/poetryEntries/GuidanceGadgets.cpp src/poetryEntries/PoetryBindings.cpp src/poetryEntries/PoetryBlocks.cpp
  src/poetryEntries/SystemHead.cpp src/poetryEntries/SystemPoetryFile.cpp
  ${CMAKE_BINARY_DIR}/generated/poetryEntries/SystemPoetryDefault.cpp
  src/poetryEntries/AsyncSections.cpp
  src/poetryEntries/AsyncPoetryStart.cpp
  src/poetryEntries/AsyncPoetryDynamic.cpp src/poetryEntries/AsyncPoetryFinish.cpp
  src/poetryEntries/EffectiveSystemPoetry.cpp)
target_link_libraries(batbox_poetryEntries PUBLIC batbox_context batbox_core)
target_compile_options(batbox_poetryEntries PRIVATE -Wall -Wextra -Wpedantic)
if(WIN32)
  set(BATBOX_INDIGENOUS_EXEC_SOURCE src/process/IndigenousExecUv.cpp)
else()
  set(BATBOX_INDIGENOUS_EXEC_SOURCE src/process/IndigenousExec.cpp)
endif()
add_library(batbox_process src/process/ExecFileNoThrow.cpp ${BATBOX_INDIGENOUS_EXEC_SOURCE})
target_link_libraries(batbox_process PUBLIC batbox_event_loop batbox_core)
target_compile_options(batbox_process PRIVATE -Wall -Wextra -Wpedantic)
# One git system, one target. src/git/ holds the subprocess/binding half,
# src/git/filesystem/ the filesystem/state half; GitBindings.hpp bridges
# the two halves, so one target is the honest shape.
add_library(batbox_git src/git/filesystem/Paths.cpp src/git/filesystem/Refs.cpp
  src/git/filesystem/Computations.cpp src/git/filesystem/Watcher.cpp
  src/git/filesystem/IndigenousFs.cpp
  src/git/GitBasics.cpp src/git/IndigenousBasics.cpp
  src/git/GitBindings.cpp src/git/CanonicalRoot.cpp src/git/IndigenousCanonicalRoot.cpp)
target_link_libraries(batbox_git PUBLIC batbox_event_loop batbox_core ICU::i18n
  batbox_context batbox_process OpenSSL::Crypto ICU::uc)
target_compile_options(batbox_git PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_shell PRIVATE src/gadgets/bash/AsyncBashPersistence.cpp
  src/gadgets/bash/AsyncBashImage.cpp)
add_library(batbox_shell_media src/gadgets/bash/IndigenousBashImage.cpp)
target_link_libraries(batbox_shell_media PUBLIC batbox_shell batbox_media)
target_compile_options(batbox_shell_media PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_query PRIVATE
  src/query/MemoryAttachmentFilter.cpp
  src/query/SimpleQueriesBinding.cpp
  src/query/OwnerWatchdogTimer.cpp
  src/query/AsyncCompletionStart.cpp
  src/query/AsyncCompletionStop.cpp src/query/AsyncGadgetTurnStart.cpp
  src/query/AsyncGadgetTurnAbort.cpp src/query/AsyncGadgetTurnAttachments.cpp
  src/query/AsyncGadgetTurnFinish.cpp src/query/IndigenousModelCursor.cpp
  src/query/IndigenousGadgetCursor.cpp src/query/AsyncOperationQueue.cpp
  src/query/AsyncEntry.cpp
  src/query/AsyncEngineClose.cpp src/query/AsyncModelClose.cpp
  src/query/AsyncDecisionClose.cpp src/query/OwnerPreflight.cpp
  src/query/OwnerModelCursor.cpp src/query/OwnerStreamConsume.cpp src/query/OwnerRetry.cpp)
