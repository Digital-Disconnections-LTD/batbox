add_library(batbox_indigenous_host STATIC
  src/application/HostCore.cpp
  src/application/HostBackend.cpp src/application/HostFrontend.cpp
  src/application/ModelBinding.cpp src/application/HostMaintenance.cpp
  src/application/HostShutdown.cpp src/application/ErrorReport.cpp
  src/application/HostCron.cpp src/application/HostShellSnapshot.cpp
  src/application/HostRetention.cpp)
target_link_libraries(batbox_indigenous_host PUBLIC
  batbox_profiler
  batbox_application_config batbox_application_model
  batbox_application_engine batbox_application_flights
  batbox_application_shell batbox_application_connectors
  batbox_application_poetry batbox_application_bats
  batbox_application_hooks batbox_application_permissions
  batbox_application_gadgets batbox_application_runtime
  batbox_application_frontend batbox_application_presentation
  batbox_application_commands batbox_application_restore batbox_application_cli
  batbox_application_memory batbox_providers batbox_diagnostics
  batbox_cron batbox_cron_gadgets)
target_compile_options(batbox_indigenous_host PRIVATE -Wall -Wextra -Wpedantic)
set(BATBOX_SETUP_WEB_SOURCE ${CMAKE_SOURCE_DIR}/gadgets/setup-web.sh)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BATBOX_SETUP_WEB_SOURCE})
file(READ ${BATBOX_SETUP_WEB_SOURCE} BATBOX_SETUP_WEB_SCRIPT)
configure_file(src/application/SetupWebScript.hpp.in
  ${CMAKE_BINARY_DIR}/generated/application/SetupWebScript.hpp @ONLY)
add_library(batbox_subcommands STATIC src/application/Subcommands.cpp)
target_include_directories(batbox_subcommands PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(batbox_subcommands PUBLIC batbox_application_config
  batbox_application_connectors batbox_providers batbox_poetryEntries batbox_instructions)
target_compile_options(batbox_subcommands PRIVATE -Wall -Wextra)
add_executable(batbox src/application/main.cpp)
target_link_libraries(batbox PRIVATE batbox_indigenous_host batbox_subcommands)
# `batbox version` also names the commit and day the build was configured.
# A checkout asks git; a release archive has no history, so `git archive`
# writes the commit into cmake/ArchiveCommit.txt (see .gitattributes).
# Packagers can still pass -DBATBOX_GIT_COMMIT=...; SOURCE_DATE_EPOCH, when
# set, fixes the date for reproducible builds.
if(NOT BATBOX_GIT_COMMIT)
  set(BATBOX_DETECTED_COMMIT "")
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    find_package(Git QUIET)
    if(GIT_FOUND)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE BATBOX_GIT_STATUS
        OUTPUT_VARIABLE BATBOX_DETECTED_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(NOT BATBOX_GIT_STATUS EQUAL 0)
        set(BATBOX_DETECTED_COMMIT "")
      endif()
    endif()
  else()
    file(STRINGS "${CMAKE_SOURCE_DIR}/cmake/ArchiveCommit.txt" BATBOX_ARCHIVE_COMMIT
      LIMIT_COUNT 1 REGEX "^[0-9a-f]+$")
    if(BATBOX_ARCHIVE_COMMIT MATCHES "^[0-9a-f]+$")
      string(SUBSTRING "${BATBOX_ARCHIVE_COMMIT}" 0 12 BATBOX_DETECTED_COMMIT)
    endif()
  endif()
  if(BATBOX_DETECTED_COMMIT)
    set(BATBOX_BUILD_COMMIT "${BATBOX_DETECTED_COMMIT}")
  else()
    set(BATBOX_BUILD_COMMIT "unknown")
  endif()
else()
  set(BATBOX_BUILD_COMMIT "${BATBOX_GIT_COMMIT}")
endif()
string(TIMESTAMP BATBOX_BUILD_DATE "%Y-%m-%d" UTC)
target_compile_definitions(batbox PRIVATE BATBOX_VERSION="${PROJECT_VERSION}"
  BATBOX_COMMIT="${BATBOX_BUILD_COMMIT}" BATBOX_BUILD_DATE="${BATBOX_BUILD_DATE}")
if(MINGW)
  target_link_options(batbox PRIVATE -municode)
endif()
# Release links drop the functions nothing reaches; the install and packaging
# steps strip the symbol table afterwards.
if(APPLE)
  target_link_options(batbox PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(batbox PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>)
endif()
include(cmake/IndigenousLsp.cmake)
