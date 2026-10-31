add_library(batbox_poetry_submit
  src/interactive/submit/CommandQueueCore.cpp
  src/interactive/submit/CommandQueueEditable.cpp
  src/interactive/submit/ExecuteFinish.cpp
  src/interactive/submit/ExecuteInput.cpp
  src/interactive/submit/PoetrySubmitDirect.cpp
  src/interactive/submit/QueueProcessor.cpp
  src/interactive/submit/QueueReadiness.cpp
  src/interactive/submit/Workload.cpp
  src/interactive/TurnBinding.cpp)
target_link_libraries(batbox_poetry_submit PUBLIC batbox_user_input
  batbox_interactive batbox_history batbox_event_loop)
target_compile_options(batbox_poetry_submit PRIVATE -Wall -Wextra -Wpedantic)
