add_library(batbox_terminal_render
  src/terminal_render/CliBoxes.cpp
  src/terminal_render/LayoutWidth.cpp
  src/terminal_render/IndigenousIndent.cpp
  src/terminal_render/RenderBorder.cpp
  src/terminal_render/RenderNodeBox.cpp
  src/terminal_render/RenderNodeCore.cpp
  src/terminal_render/RenderNodeHelpers.cpp
  src/terminal_render/RenderNodeText.cpp
  src/terminal_render/RenderScrollFast.cpp
  src/terminal_render/RenderScrollState.cpp
  src/terminal_render/RenderState.cpp
  src/terminal_render/RenderToScreen.cpp
  src/terminal_render/ScreenSearch.cpp
  src/terminal_render/TextIndigenous.cpp
  src/terminal_render/TextSegments.cpp
  src/terminal_render/TextWrap.cpp
  src/terminal_render/WrappedStyles.cpp
)
target_link_libraries(batbox_terminal_render PUBLIC batbox_terminal_output batbox_terminal_renderer batbox_terminal_dom batbox_text_width ICU::uc)
target_compile_options(batbox_terminal_render PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_color
  src/terminal_color/ChalkColor.cpp
  src/terminal_color/ChalkWrap.cpp
  src/terminal_color/ColorBinding.cpp
  src/terminal_color/Colorize.cpp
  src/terminal_color/IndigenousColorDetection.cpp
  src/terminal_color/SupportsColor.cpp
)
target_link_libraries(batbox_terminal_color PUBLIC batbox_terminal_render)
target_compile_options(batbox_terminal_color PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_hit_test
  src/terminal_hit_test/DispatchHover.cpp
  src/terminal_hit_test/HitTest.cpp
  src/terminal_hit_test/PointerHandlerStore.cpp
)
target_link_libraries(batbox_terminal_hit_test PUBLIC batbox_terminal_dom)
target_compile_options(batbox_terminal_hit_test PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_reconciler
  src/terminal_reconciler/CommitLifecycle.cpp
  src/terminal_reconciler/HostCommit.cpp
  src/terminal_reconciler/HostConfigIndigenous.cpp
  src/terminal_reconciler/HostInstances.cpp
  src/terminal_reconciler/HostLeaves.cpp
  src/terminal_reconciler/HostProps.cpp
  src/terminal_reconciler/HostPropsIndigenous.cpp
  src/terminal_reconciler/HostStatic.cpp
  src/terminal_reconciler/HostStaticIndigenous.cpp
)
target_link_libraries(batbox_terminal_reconciler PUBLIC batbox_terminal_style batbox_terminal_hit_test)
target_compile_options(batbox_terminal_reconciler PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_optimizer
  src/terminal_optimizer/Optimizer.cpp
)
target_link_libraries(batbox_terminal_optimizer PUBLIC batbox_terminal_renderer)
target_compile_options(batbox_terminal_optimizer PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_selection
  src/terminal_selection/SelectionOverlay.cpp
  src/terminal_selection/SelectionShift.cpp
  src/terminal_selection/SelectionState.cpp
  src/terminal_selection/SelectionText.cpp
  src/terminal_selection/SelectionUrl.cpp
  src/terminal_selection/SelectionWord.cpp
)
target_link_libraries(batbox_terminal_selection PUBLIC batbox_terminal_output ICU::uc)
target_compile_options(batbox_terminal_selection PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_log_update
  src/terminal_log_update/CellWrite.cpp
  src/terminal_log_update/FrameSlice.cpp
  src/terminal_log_update/FullReset.cpp
  src/terminal_log_update/LogUpdateBase.cpp
  src/terminal_log_update/PatchBridge.cpp
  src/terminal_log_update/PatchJson.cpp
  src/terminal_log_update/Render.cpp
  src/terminal_log_update/Text.cpp
  src/terminal_log_update/VirtualScreen.cpp
)
target_link_libraries(batbox_terminal_log_update PUBLIC batbox_terminal_screen batbox_terminal_renderer batbox_termio batbox_ansi_tokenize batbox_intl)
target_compile_options(batbox_terminal_log_update PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_runtime
  src/terminal_runtime/TerminalAlternate.cpp
  src/terminal_runtime/TerminalControlCore.cpp
  src/terminal_runtime/TerminalDrainIndigenous.cpp
  src/terminal_runtime/TerminalExitPromise.cpp
  src/terminal_runtime/TerminalInteraction.cpp
  src/terminal_runtime/TerminalSearch.cpp
  src/terminal_runtime/TerminalSelection.cpp
  src/terminal_runtime/TerminalStdin.cpp
  src/terminal_runtime/TerminalUnmount.cpp
)
target_link_libraries(batbox_terminal_runtime PUBLIC batbox_terminal_log_update batbox_terminal_color batbox_terminal_reconciler batbox_terminal_selection batbox_terminal_optimizer batbox_clipboard batbox_keyboard_dispatch)
target_compile_options(batbox_terminal_runtime PRIVATE -Wall -Wextra -Wpedantic)
