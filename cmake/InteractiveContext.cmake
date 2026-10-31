target_sources(batbox_interactive PRIVATE
  src/interactive/context/GadgetPool.cpp
  src/interactive/context/ContextFactoryCore.cpp
  src/interactive/context/ContextFactoryCallbacks.cpp
  src/interactive/context/ContextProjection.cpp)
target_link_libraries(batbox_interactive PUBLIC batbox_bats)
