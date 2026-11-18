add_library(batbox_interactive
  src/interactive/CommandUi.cpp src/interactive/StreamReducer.cpp
  src/interactive/PresentationState.cpp src/interactive/TranscriptRows.cpp
  src/interactive/QueryGuard.cpp src/interactive/QueryTurnStart.cpp
  src/interactive/QueryTurnFinally.cpp
  src/interactive/QueryExecutionStart.cpp src/interactive/QueryExecutionContext.cpp
  src/interactive/QueryExecutionConsume.cpp)
target_link_libraries(batbox_interactive PUBLIC batbox_flights batbox_event_loop
  batbox_app_state batbox_poetryEntries batbox_query batbox_user_input
  batbox_history batbox_clipboard batbox_terminal_events
  batbox_keyboard_input batbox_bat_context)
target_compile_options(batbox_interactive PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/MessageList.cmake)
include(cmake/InteractiveContext.cmake)
# Input line, submit queue, lifecycle/navigation/repl runtime, and the
# background-attach helper (formerly batbox_text_editor, batbox_poetry_submit,
# batbox_interactive_lifecycle, batbox_background_flight). They were cmake
# slices of this same directory carved for link bookkeeping, not boundaries;
# none reference ftxui, so all compile on the presentation-model side.
target_sources(batbox_interactive PRIVATE
  src/interactive/text_input/CursorCore.cpp
  src/interactive/text_input/CursorEdit.cpp
  src/interactive/text_input/CursorRender.cpp
  src/interactive/text_input/CursorWords.cpp
  src/interactive/text_input/KillRing.cpp
  src/interactive/text_input/MeasuredTextCore.cpp
  src/interactive/text_input/MeasuredTextLines.cpp
  src/interactive/text_input/MeasuredTextIndigenous.cpp
  src/interactive/text_input/TextEditorCore.cpp
  src/interactive/text_input/TextEditorKeys.cpp
  src/interactive/text_input/TextEditorIndigenous.cpp
  src/interactive/submit/CommandQueueCore.cpp
  src/interactive/submit/CommandQueueEditable.cpp
  src/interactive/submit/ExecuteFinish.cpp
  src/interactive/submit/ExecuteInput.cpp
  src/interactive/submit/PoetrySubmitDirect.cpp
  src/interactive/submit/QueueProcessor.cpp
  src/interactive/submit/QueueReadiness.cpp
  src/interactive/submit/QueuedInputs.cpp
  src/interactive/submit/Workload.cpp
  src/interactive/TurnBinding.cpp
  src/interactive/lifecycle/MessageState.cpp
  src/interactive/lifecycle/BashGadgets.cpp
  src/interactive/lifecycle/LoadingState.cpp
  src/interactive/navigation/ResubmitText.cpp
  src/interactive/navigation/RewindCore.cpp
  src/interactive/navigation/MessageCopy.cpp
  src/interactive/navigation/MessageCopyBinding.cpp
  src/interactive/repl_runtime/RuntimeCore.cpp
  src/interactive/repl_runtime/RuntimeDerived.cpp
  src/interactive/repl_runtime/RuntimeIngress.cpp
  src/interactive/repl_runtime/RuntimeInit.cpp
  src/interactive/repl_runtime/RuntimeTranscript.cpp
  src/interactive/background/NotificationAttachments.cpp)

add_library(batbox_terminal_ftxui
  src/terminal_ftxui/Driver.cpp src/terminal_ftxui/PresentationLayout.cpp
  src/terminal_ftxui/TranscriptCopy.cpp
  src/terminal_ftxui/PresentationView.cpp src/terminal_ftxui/StyleProjection.cpp
  src/terminal_ftxui/IndigenousStringMetrics.cpp
  src/terminal_ftxui/ScrollbackProjection.cpp)
target_link_libraries(batbox_terminal_ftxui PUBLIC batbox_interactive
  ftxui::component ftxui::dom ftxui::screen batbox_text_sdk batbox_intl
  batbox_user_input batbox_history)
target_compile_options(batbox_terminal_ftxui PRIVATE -Wall -Wextra -Wpedantic)
# The conversation shell, its presentation providers, and the completion /
# external-editor / history-line bindings that exist only to serve it
# (formerly batbox_conversation_shell). This is the ftxui-rendered half of
# the UI -- the seam between Shell/viewport code and the ftxui-free
# presentation model above, so it compiles on the driver side.
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/conversation_shell/ShellCore.cpp
  src/interactive/conversation_shell/ShellConversation.cpp
  src/interactive/conversation_shell/ShellRender.cpp
  src/interactive/conversation_shell/PoetryViewport.cpp
  src/interactive/conversation_shell/ShellInput.cpp
  src/interactive/conversation_shell/ShellTranscript.cpp
  src/interactive/presentation_providers/ThemeLight.cpp
  src/interactive/presentation_providers/ThemeDark.cpp
  src/interactive/presentation_providers/ThemeDaltonized.cpp
  src/interactive/presentation_providers/ThemeData.cpp
  src/interactive/presentation_providers/ThemeBatbox.cpp
  src/interactive/presentation_providers/StyleRoles.cpp
  src/interactive/presentation_providers/SystemTheme.cpp
  src/interactive/presentation_providers/ThemeState.cpp
  src/interactive/presentation_providers/Shortcut.cpp
  src/interactive/presentation_providers/DefaultBindings.cpp
  src/interactive/presentation_providers/Chrome.cpp
  src/interactive/presentation_providers/MissionFooter.cpp
  src/interactive/presentation_providers/CardPalettes.cpp
  src/application/completion/Commands.cpp
  src/application/completion/Paths.cpp
  src/application/editor/IndigenousProcess.cpp
  src/application/editor/PoetryEditor.cpp
  src/application/editor/TranscriptFile.cpp
  src/application/history/History.cpp)
include(cmake/InteractiveCards.cmake)
include(cmake/MessageView.cmake)
include(cmake/MessageListView.cmake)
include(cmake/IndigenousMarkdown.cmake)
