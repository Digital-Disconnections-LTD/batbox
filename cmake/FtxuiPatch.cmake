# Preserve the application's input protocol while FTXUI alone owns terminal I/O.
# Apply after MakeAvailable as well as for offline source-directory overrides.
find_package(Git REQUIRED)
set(batbox_ftxui_patch_names
    ftxui-7.0.3-raw-input.patch ftxui-7.0.3-string-metrics.patch
    ftxui-7.0.3-inline-scrollback.patch ftxui-7.0.3-bracketed-paste.patch
    ftxui-7.0.3-selection-spaces.patch)
set(batbox_ftxui_signature_files)
foreach(name IN LISTS batbox_ftxui_patch_names)
  set(patch "${CMAKE_CURRENT_LIST_DIR}/../src/terminal_ftxui/patches/${name}")
  list(APPEND batbox_ftxui_signature_files "${patch}")
  file(STRINGS "${patch}" targets REGEX "^\\+\\+\\+ b/")
  foreach(target IN LISTS targets)
    string(REGEX REPLACE "^\\+\\+\\+ b/" "" target "${target}")
    list(APPEND batbox_ftxui_signature_files "${batbox_ftxui_upstream_SOURCE_DIR}/${target}")
  endforeach()
endforeach()
list(REMOVE_DUPLICATES batbox_ftxui_signature_files)
function(batbox_ftxui_signature output)
  set(value "")
  foreach(path IN LISTS batbox_ftxui_signature_files)
    file(SHA256 "${path}" digest)
    string(APPEND value "${path}:${digest}\n")
  endforeach()
  set(${output} "${value}" PARENT_SCOPE)
endfunction()
set(batbox_ftxui_stamp "${batbox_ftxui_upstream_SOURCE_DIR}/.batbox-adapter-signature")
batbox_ftxui_signature(current_signature)
if(EXISTS "${batbox_ftxui_stamp}")
  file(READ "${batbox_ftxui_stamp}" previous_signature)
  if(current_signature STREQUAL previous_signature)
    return()
  endif()
endif()

# Later patches can overlap an earlier patch's context. Unwind the applied
# stack in reverse order before checking it; checking each patch against the
# fully patched tree incorrectly rejects a second configure.
set(batbox_ftxui_reverse_names ${batbox_ftxui_patch_names})
list(REVERSE batbox_ftxui_reverse_names)
foreach(name IN LISTS batbox_ftxui_reverse_names)
  set(patch "${CMAKE_CURRENT_LIST_DIR}/../src/terminal_ftxui/patches/${name}")
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
    WORKING_DIRECTORY "${batbox_ftxui_upstream_SOURCE_DIR}"
    RESULT_VARIABLE applied OUTPUT_QUIET ERROR_QUIET)
  if(applied EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse "${patch}"
      WORKING_DIRECTORY "${batbox_ftxui_upstream_SOURCE_DIR}"
      COMMAND_ERROR_IS_FATAL ANY)
  endif()
endforeach()
foreach(batbox_ftxui_patch_name IN LISTS batbox_ftxui_patch_names)
set(batbox_ftxui_patch
  "${CMAKE_CURRENT_LIST_DIR}/../src/terminal_ftxui/patches/${batbox_ftxui_patch_name}")
execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${batbox_ftxui_patch}"
  WORKING_DIRECTORY "${batbox_ftxui_upstream_SOURCE_DIR}"
  RESULT_VARIABLE batbox_ftxui_already_patched
  OUTPUT_QUIET ERROR_QUIET)
if(NOT batbox_ftxui_already_patched EQUAL 0)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${batbox_ftxui_patch}"
    WORKING_DIRECTORY "${batbox_ftxui_upstream_SOURCE_DIR}"
    RESULT_VARIABLE batbox_ftxui_check
    OUTPUT_QUIET ERROR_VARIABLE batbox_ftxui_patch_error)
  if(NOT batbox_ftxui_check EQUAL 0)
    message(FATAL_ERROR "FTXUI adapter ${batbox_ftxui_patch_name} does not apply: ${batbox_ftxui_patch_error}")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply "${batbox_ftxui_patch}"
    WORKING_DIRECTORY "${batbox_ftxui_upstream_SOURCE_DIR}"
    RESULT_VARIABLE batbox_ftxui_apply
    OUTPUT_QUIET ERROR_VARIABLE batbox_ftxui_patch_error)
  if(NOT batbox_ftxui_apply EQUAL 0)
    message(FATAL_ERROR "FTXUI adapter ${batbox_ftxui_patch_name} failed: ${batbox_ftxui_patch_error}")
  endif()
endif()
endforeach()
batbox_ftxui_signature(current_signature)
file(WRITE "${batbox_ftxui_stamp}" "${current_signature}")
