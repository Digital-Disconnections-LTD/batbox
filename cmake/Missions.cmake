# The missions system: the background-mission roster, its output gadget,
# the stop gadget, and the mission store. It was scattered -- roster, output
# and stop compiled inside batbox_shell, the store a thin standalone target
# -- while the name-shaped dirs (src/missions/, src/gadgets/mission_*) sat
# apart from their target. It includes shell headers (ManagedShell, the
# output poller); the shell declares the cycle back.
add_library(batbox_missions
  src/missions/shell/Roster.cpp
  src/gadgets/mission_output/MissionOutput.cpp
  src/gadgets/mission_output/Formatting.cpp
  src/gadgets/mission_output/Data.cpp
  src/gadgets/mission_stop/MissionStop.cpp
  src/gadgets/mission_stop/Result.cpp
  src/gadgets/mission_management/Store.cpp
  src/gadgets/mission_management/PoetryEntries.cpp
  src/gadgets/mission_management/CreateGet.cpp
  src/gadgets/mission_management/Update.cpp
  src/gadgets/mission_management/List.cpp)
target_link_libraries(batbox_missions PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_missions PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
  add_executable(batbox-mission-management-indigenous
    tests/phase5/mission_management_indigenous.cpp)
  target_link_libraries(batbox-mission-management-indigenous PRIVATE batbox_missions)
  if(MSVC)
    target_compile_options(batbox-mission-management-indigenous PRIVATE /UNDEBUG)
  else()
    target_compile_options(batbox-mission-management-indigenous PRIVATE -UNDEBUG)
  endif()
  add_test(NAME batbox-mission-management-indigenous COMMAND batbox-mission-management-indigenous)
endif()
