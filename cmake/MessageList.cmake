target_sources(batbox_interactive PRIVATE
  src/interactive/MessageOrder.cpp src/interactive/MessageVisibility.cpp
  src/interactive/MessageDecisions.cpp src/interactive/MessageGrouping.cpp
  src/interactive/MessageCollapse.cpp src/interactive/MessageBackgroundCollapse.cpp
  src/interactive/MessageTags.cpp src/interactive/MessageLookups.cpp
  src/interactive/MessageListPipeline.cpp src/interactive/MessageNormalizationRefs.cpp
  src/interactive/MessageSynthetic.cpp src/interactive/GadgetClassification.cpp
  src/interactive/ReadSearchClassification.cpp src/interactive/ReadSearchGroup.cpp
  src/interactive/ReadSearchGit.cpp src/interactive/ReadSearchHints.cpp
  src/interactive/ReadSearchCollapse.cpp)
target_link_libraries(batbox_interactive PUBLIC batbox_glob)
