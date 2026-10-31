add_library(batbox_indigenous_lsp
  src/lsp/Protocol.cpp src/lsp/Client.cpp src/lsp/Diagnostics.cpp src/lsp/Manager.cpp
  src/application_lsp/Format.cpp src/application_lsp/Runtime.cpp
  src/application_lsp/Gadget.cpp src/application_lsp/Symbol.cpp)
target_link_libraries(batbox_indigenous_lsp PUBLIC batbox_core batbox_indigenous_jsonl_process)
target_compile_options(batbox_indigenous_lsp PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_indigenous_lsp)
target_link_libraries(batbox_indigenous_host PUBLIC batbox_indigenous_lsp)
