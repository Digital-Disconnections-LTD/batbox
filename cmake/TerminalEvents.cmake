add_library(batbox_terminal_events src/terminal_events/Event.cpp
  src/terminal_events/KeyboardEvent.cpp src/terminal_events/IndigenousClock.cpp
  src/terminal_events/Dispatcher.cpp src/terminal_events/HandlerMap.cpp)
target_sources(batbox_terminal_events PRIVATE src/terminal_events/Emitter.cpp src/terminal_events/InputEvent.cpp)
target_link_libraries(batbox_terminal_events PUBLIC batbox_core batbox_keyboard_input)
target_compile_options(batbox_terminal_events PRIVATE -Wall -Wextra -Wpedantic)
