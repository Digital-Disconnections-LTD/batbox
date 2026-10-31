foreach(section IN ITEMS terminal_render_state terminal_screen_search terminal_styled_text terminal_indent_sdk terminal_border terminal_render_node terminal_render_to_screen)
  batbox_probe(${section} batbox_terminal_render phase6)
endforeach()
foreach(section IN ITEMS terminal_color terminal_color_detection)
  batbox_probe(${section} batbox_terminal_color phase6)
endforeach()
foreach(section IN ITEMS terminal_reconciler_leaves terminal_reconciler_props terminal_reconciler_host terminal_reconciler_commit terminal_reconciler_static)
  batbox_probe(${section} batbox_terminal_reconciler phase6)
endforeach()
batbox_indigenous(phase6-terminal-reconciler-static-indigenous tests/phase6/terminal_reconciler_static_indigenous.cpp batbox_terminal_reconciler)
batbox_probe(terminal_hit_test batbox_terminal_hit_test phase6)
batbox_probe(terminal_optimizer batbox_terminal_optimizer phase6)
foreach(section IN ITEMS terminal_selection_state terminal_selection_screen)
  batbox_probe(${section} batbox_terminal_selection phase6)
endforeach()
batbox_probe(terminal_log_update batbox_terminal_log_update phase6)
batbox_probe(terminal_log_update_helpers batbox_terminal_log_update phase6)
# This fixture injects screen and terminal services for isolated control checks.
add_executable(batbox-indigenous_ink_controls-probe
  tests/phase5/indigenous_ink_controls_probe.cpp
  src/terminal_runtime/SearchHighlight.cpp
  src/terminal_runtime/TerminalAlternate.cpp
  src/terminal_runtime/TerminalControlCore.cpp
  src/terminal_runtime/TerminalDrainIndigenous.cpp
  src/terminal_runtime/TerminalExitPromise.cpp
  src/terminal_runtime/TerminalInteraction.cpp
  src/terminal_runtime/TerminalOwnerRoot.cpp
  src/terminal_runtime/TerminalRenderCycle.cpp
  src/terminal_runtime/TerminalSearch.cpp
  src/terminal_runtime/TerminalSelection.cpp
  src/terminal_runtime/TerminalStdin.cpp
  src/terminal_runtime/TerminalUnmount.cpp
  src/terminal_selection/SelectionShift.cpp
  src/terminal_selection/SelectionState.cpp
  src/terminal_selection/SelectionText.cpp
  src/terminal_selection/SelectionUrl.cpp
  src/terminal_selection/SelectionWord.cpp
  src/terminal_render/RenderState.cpp
  src/terminal_renderer/Frame.cpp src/terminal_optimizer/Optimizer.cpp)
target_compile_options(batbox-indigenous_ink_controls-probe PRIVATE -ffunction-sections -fdata-sections)
if(APPLE)
  target_link_options(batbox-indigenous_ink_controls-probe PRIVATE -Wl,-dead_strip)
else()
  target_link_options(batbox-indigenous_ink_controls-probe PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(batbox-indigenous_ink_controls-probe PRIVATE batbox_termio batbox_event_loop ICU::uc)

batbox_indigenous(phase5-terminal-runtime-indigenous tests/phase5/terminal_runtime_indigenous.cpp batbox_terminal_runtime)
