# Concrete owners and providers used by the indigenous terminal application.
add_library(batbox_conversation_shell
  src/interactive/conversation_shell/ShellCore.cpp
  src/interactive/conversation_shell/ShellConversation.cpp
  src/interactive/conversation_shell/ShellRender.cpp
  src/interactive/conversation_shell/PoetryViewport.cpp
  src/interactive/conversation_shell/ShellInput.cpp
  src/interactive/presentation_providers/ThemeLight.cpp
  src/interactive/presentation_providers/ThemeDark.cpp
  src/interactive/presentation_providers/ThemeDaltonized.cpp
  src/interactive/presentation_providers/ThemeData.cpp
  src/interactive/presentation_providers/SystemTheme.cpp
  src/interactive/presentation_providers/ThemeState.cpp
  src/interactive/presentation_providers/Shortcut.cpp
  src/interactive/presentation_providers/DefaultBindings.cpp
  src/interactive/presentation_providers/Chrome.cpp
  src/interactive/presentation_providers/MissionFooter.cpp
  src/interactive/presentation_providers/CardPalettes.cpp)
target_link_libraries(batbox_conversation_shell PUBLIC batbox_terminal_ftxui
  batbox_interactive_lifecycle batbox_interaction_controller batbox_text_editor)
target_compile_options(batbox_conversation_shell PRIVATE -Wall -Wextra -Wpedantic)

target_sources(batbox_core_commands PRIVATE
  src/commands_indigenous/Rename.cpp src/commands_indigenous/FlightSimple.cpp
  src/commands_indigenous/PageCommands.cpp src/commands_indigenous/RemainingCatalog.cpp)
target_link_libraries(batbox_core_commands PUBLIC batbox_monster batbox_bats)

add_library(batbox_live_gadget_roster src/gadgets/live_roster/Factory.cpp
  src/gadgets/live_roster/Select.cpp src/gadgets/live_roster/ContextBinding.cpp)
target_link_libraries(batbox_live_gadget_roster PUBLIC batbox_interactive
  batbox_bats batbox_shell batbox_core batbox_web batbox_mcp)
target_compile_options(batbox_live_gadget_roster PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_config src/application/Config.cpp)
target_link_libraries(batbox_application_config PUBLIC batbox_core)
target_compile_options(batbox_application_config PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_gadgets
  src/application_gadgets/ApplicationGadgets.cpp
  src/application_gadgets/IndigenousFiles.cpp src/application_gadgets/IndigenousFileHelpers.cpp)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_live_gadget_roster
  batbox_skills batbox_media batbox_core)
target_compile_options(batbox_application_gadgets PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_runtime
  src/application_runtime/ApplicationRuntimeCore.cpp
  src/application_runtime/ApplicationBindings.cpp
  src/application_runtime/RuntimeBindings.cpp
  src/application_runtime/ShellBinding.cpp)
target_link_libraries(batbox_application_runtime PUBLIC batbox_conversation_shell
  batbox_application_config batbox_interactive_lifecycle)
target_compile_options(batbox_application_runtime PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/IndigenousApplicationBackend.cmake)
