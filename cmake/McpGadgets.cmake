find_package(ICU REQUIRED COMPONENTS uc)
add_library(batbox_mcp
  src/mcp/Call.cpp src/mcp/Codec.cpp src/mcp/Collapse.cpp
  src/mcp/CollapseRead1.cpp src/mcp/CollapseRead2.cpp src/mcp/CollapseRead3.cpp
  src/mcp/CollapseSearch.cpp src/mcp/Names.cpp src/mcp/OutputStorage.cpp
  src/mcp/ProcessResult.cpp src/mcp/ResultContent.cpp src/mcp/Sanitize.cpp
  src/mcp/StdioProcess.cpp src/mcp/StdioReader.cpp src/mcp/StdioRequest.cpp
  src/mcp/GadgetFactory.cpp src/mcp/Validation.cpp)
target_link_libraries(batbox_mcp PUBLIC batbox_core batbox_media ICU::uc Threads::Threads)
target_compile_options(batbox_mcp PRIVATE -Wall -Wextra -Wpedantic)
