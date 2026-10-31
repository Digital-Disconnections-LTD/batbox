add_library(batbox_application_skills
  src/application_skills/ProviderCore.cpp
  src/application_skills/ProviderDiscovery.cpp
  src/application_skills/ProviderPoetry.cpp
  src/application_skills/ProviderGadget.cpp
  src/application_skills/ProviderShell.cpp
  src/application_skills/ShellExpansion.cpp)
target_link_libraries(batbox_application_skills PUBLIC batbox_skills
  batbox_application_config batbox_application_model batbox_application_flights
  batbox_application_media batbox_application_hooks batbox_bootstrap_state
  batbox_user_input batbox_bats batbox_gadget_results batbox_shell)
target_compile_options(batbox_application_skills PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_bats PUBLIC batbox_application_skills)

if(BUILD_TESTING)
  add_executable(batbox-indigenous-skill-shell-format-driver
    checks/indigenous_skill_shell_format_driver.cpp)
  target_link_libraries(batbox-indigenous-skill-shell-format-driver PRIVATE
    batbox_application_skills)
  add_executable(batbox-indigenous-skill-shell-runtime
    checks/indigenous_skill_shell_runtime.cpp)
  target_link_libraries(batbox-indigenous-skill-shell-runtime PRIVATE
    batbox_application_skills batbox_indigenous_host)
  add_test(NAME phase6-indigenous-skill-shell-runtime
    COMMAND batbox-indigenous-skill-shell-runtime)

  set_tests_properties(phase6-indigenous-skill-shell-runtime PROPERTIES TIMEOUT 45)
endif()
