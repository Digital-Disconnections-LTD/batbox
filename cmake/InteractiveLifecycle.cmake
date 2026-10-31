add_library(batbox_interactive_lifecycle
  src/interactive/lifecycle/MessageState.cpp
  src/interactive/lifecycle/BashGadgets.cpp
  src/interactive/lifecycle/LoadingState.cpp
  src/interactive/lifecycle/InitialMessage.cpp
  src/interactive/lifecycle/BatSubmit.cpp)
target_link_libraries(batbox_interactive_lifecycle PUBLIC batbox_interactive
  batbox_poetry_submit batbox_app_state batbox_event_loop)
target_compile_options(batbox_interactive_lifecycle PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_interactive_lifecycle PRIVATE
  src/interactive/navigation/DialogSelection.cpp
  src/interactive/navigation/DialogFocus.cpp
  src/interactive/navigation/Cancel.cpp
  src/interactive/navigation/QueueCancel.cpp
  src/interactive/navigation/ExitFlow.cpp
  src/interactive/navigation/ResubmitText.cpp
  src/interactive/navigation/RewindCore.cpp
  src/interactive/navigation/RewindEdit.cpp
  src/interactive/navigation/MessageCopy.cpp
  src/interactive/navigation/MessageCopyBinding.cpp)
target_link_libraries(batbox_interactive_lifecycle PUBLIC batbox_clipboard)
target_sources(batbox_interactive_lifecycle PRIVATE
  src/interactive/repl_runtime/RuntimeCore.cpp
  src/interactive/repl_runtime/RuntimeDerived.cpp
  src/interactive/repl_runtime/RuntimeIdle.cpp
  src/interactive/repl_runtime/RuntimeIngress.cpp
  src/interactive/repl_runtime/RuntimeInit.cpp
  src/interactive/repl_runtime/RuntimeTranscript.cpp)
