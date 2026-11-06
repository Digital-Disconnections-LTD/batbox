add_library(batbox_bat_context
  src/bat_context/PartnerContext.cpp
  src/bat_context/PartnerIdentity.cpp
)
target_link_libraries(batbox_bat_context PUBLIC batbox_event_loop)
target_compile_options(batbox_bat_context PRIVATE -Wall -Wextra -Wpedantic)
