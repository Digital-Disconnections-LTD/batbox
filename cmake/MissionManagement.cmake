add_library(batbox_mission_management
  src/gadgets/mission_management/Store.cpp
  src/gadgets/mission_management/PoetryEntries.cpp
  src/gadgets/mission_management/CreateGet.cpp
  src/gadgets/mission_management/Update.cpp
  src/gadgets/mission_management/List.cpp)
target_link_libraries(batbox_mission_management PUBLIC batbox_core)
target_compile_options(batbox_mission_management PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_mission_management)
target_link_libraries(batbox_application_commands PUBLIC batbox_mission_management)

if(BUILD_TESTING)
  add_executable(batbox-mission-management-indigenous
    tests/phase5/mission_management_indigenous.cpp)
  target_link_libraries(batbox-mission-management-indigenous PRIVATE batbox_mission_management)
  if(MSVC)
    target_compile_options(batbox-mission-management-indigenous PRIVATE /UNDEBUG)
  else()
    target_compile_options(batbox-mission-management-indigenous PRIVATE -UNDEBUG)
  endif()
  add_test(NAME batbox-mission-management-indigenous COMMAND batbox-mission-management-indigenous)
endif()

if(BUILD_TESTING)

endif()
