# Include after batbox_application_frontend and before IndigenousHost.
add_library(batbox_application_media
  src/application_media/Bindings.cpp
  src/application_media/ClipboardImage.cpp
  src/application_media/Attachments.cpp
  src/application_media/AttachmentAggregate.cpp
  src/application_media/AttachmentCollectors.cpp)
target_link_libraries(batbox_application_media PUBLIC batbox_media
  batbox_clipboard batbox_attachments batbox_interactive_lifecycle
  batbox_process batbox_memory_prefetch batbox_skills batbox_bootstrap_state)
target_compile_options(batbox_application_media PRIVATE -Wall -Wextra -Wpedantic)
if(APPLE)
  find_library(BATBOX_APPKIT_FRAMEWORK AppKit REQUIRED)
  target_sources(batbox_application_media PRIVATE
    src/application_media/ClipboardImageMac.mm)
  target_link_libraries(batbox_application_media PRIVATE
    ${BATBOX_APPKIT_FRAMEWORK})
endif()
target_sources(batbox_application_frontend PRIVATE src/application_frontend/Editing.cpp)
target_link_libraries(batbox_application_frontend PUBLIC batbox_application_media)
target_link_libraries(batbox_conversation_shell PUBLIC batbox_history)
