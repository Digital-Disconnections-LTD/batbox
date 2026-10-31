add_library(batbox_user_input
  src/commands/IndigenousPaths.cpp src/commands/PoetryBinding.cpp
  src/commands/Roster.cpp src/user_input/BashInput.cpp
  src/user_input/BashIndigenousBinding.cpp src/user_input/CommandMessages.cpp
  src/user_input/InputCore.cpp src/user_input/InputHooks.cpp
  src/user_input/InputPasted.cpp src/user_input/InputRoute.cpp
  src/user_input/SlashDispatch.cpp src/user_input/SlashHelpers.cpp
  src/user_input/SlashKnown.cpp src/user_input/SlashLocal.cpp
  src/user_input/SlashUi.cpp src/user_input/TextPoetry.cpp
  src/user_input/UiCommandRouter.cpp src/user_input/ImmediateUiBinding.cpp
  src/commands/fork/ForkCommand.cpp src/commands/fork/Foreground.cpp
  src/commands/fork/Background.cpp src/commands/fork/Runner.cpp)
target_link_libraries(batbox_user_input PUBLIC batbox_core batbox_event_loop
  batbox_instruction_async batbox_skills batbox_interactive batbox_shell
  batbox_gadget_results batbox_bats)
target_compile_options(batbox_user_input PRIVATE -Wall -Wextra -Wpedantic)
