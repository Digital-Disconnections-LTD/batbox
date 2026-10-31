# Indigenous extension: concise current project state, with a literal Makefile graph.
add_library(batbox_project_recovery
  src/project_recovery/Discovery.cpp src/project_recovery/Load.cpp
  src/project_recovery/Init.cpp src/project_recovery/Templates.cpp
  src/project_recovery/Text.cpp)
target_include_directories(batbox_project_recovery PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(batbox_project_recovery PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(batbox_project_recovery PRIVATE /W4)
else()
  target_compile_options(batbox_project_recovery PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
  add_executable(batbox-project-recovery-indigenous tests/phase6/project_recovery_indigenous.cpp)
  target_link_libraries(batbox-project-recovery-indigenous PRIVATE batbox_project_recovery)
  add_test(NAME phase6-project-recovery-indigenous COMMAND batbox-project-recovery-indigenous)
endif()
