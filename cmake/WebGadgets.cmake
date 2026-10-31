add_library(batbox_web
  src/gadgets/web/Factories.cpp src/gadgets/web/FetchStash.cpp src/gadgets/web/FetchCall.cpp
  src/gadgets/web/FetchModel.cpp src/gadgets/web/FetchPermission.cpp src/gadgets/web/FetchURL.cpp
  src/gadgets/web/ModelAdapter.cpp src/gadgets/web/Parse.cpp src/gadgets/web/PoetryEntries.cpp
  src/gadgets/web/Schemas.cpp src/gadgets/web/ScraplingFetch.cpp src/gadgets/web/SearchCore.cpp)
target_link_libraries(batbox_web PUBLIC batbox_connectors batbox_model batbox_core)
target_compile_options(batbox_web PRIVATE -Wall -Wextra -Wpedantic)
