add_library(batbox_attachments
  src/attachments/Aggregate.cpp
  src/attachments/Messages.cpp
  src/attachments/Leaves.cpp)
target_link_libraries(batbox_attachments PUBLIC batbox_attachment_triggers
  batbox_background_flight batbox_bootstrap_state batbox_query batbox_event_loop)
target_compile_options(batbox_attachments PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_attachments PRIVATE
  src/attachments/files/Mentions.cpp
  src/attachments/files/PDFReference.cpp
  src/attachments/files/Generate.cpp
  src/attachments/files/AtMentions.cpp
  src/attachments/files/Ide.cpp
  src/attachments/files/IndigenousBinding.cpp
  src/attachments/files/IndigenousIdeMemory.cpp)
target_link_libraries(batbox_attachments PUBLIC batbox_core batbox_glob batbox_media)
target_sources(batbox_attachments PRIVATE
  src/attachments/context/BatListing.cpp
  src/attachments/context/ContextBinding.cpp
  src/attachments/context/DeferredGadgets.cpp
  src/attachments/context/McpInstructions.cpp
  src/attachments/context/Sort.cpp)
target_link_libraries(batbox_attachments PUBLIC batbox_bats batbox_settings ICU::i18n)
target_sources(batbox_attachments PRIVATE
  src/attachments/local/ChangedFiles.cpp
  src/attachments/local/DiagnosticsMissions.cpp
  src/attachments/local/LocalBinding.cpp
  src/attachments/local/IndigenousBinding.cpp
  src/attachments/local/PlanReminders.cpp
  src/attachments/local/Snippet.cpp
  src/attachments/local/MissionReminders.cpp)
