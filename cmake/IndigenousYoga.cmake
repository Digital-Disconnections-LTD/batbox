add_library(batbox_indigenous_yoga
  src/indigenous_yoga/Absolute.cpp
  src/indigenous_yoga/Axis.cpp
  src/indigenous_yoga/Counters.cpp
  src/indigenous_yoga/Edges.cpp
  src/indigenous_yoga/FlexBasis.cpp
  src/indigenous_yoga/FlexResolve.cpp
  src/indigenous_yoga/LayoutStash.cpp
  src/indigenous_yoga/LayoutContainer.cpp
  src/indigenous_yoga/LayoutHelpers.cpp
  src/indigenous_yoga/LayoutNode.cpp
  src/indigenous_yoga/LayoutPosition.cpp
  src/indigenous_yoga/Module.cpp
  src/indigenous_yoga/NodeCore.cpp
  src/indigenous_yoga/NodeDimensions.cpp
  src/indigenous_yoga/NodeFlex.cpp
  src/indigenous_yoga/NodeGetters.cpp
  src/indigenous_yoga/NodeSpacing.cpp
  src/indigenous_yoga/Round.cpp
  src/indigenous_yoga/Values.cpp
)
target_link_libraries(batbox_indigenous_yoga PUBLIC batbox_event_loop batbox_core)
target_compile_options(batbox_indigenous_yoga PRIVATE -Wall -Wextra -Wpedantic)
