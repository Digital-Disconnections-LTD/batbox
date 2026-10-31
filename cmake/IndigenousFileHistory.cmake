# Include after application flights/gadgets/frontend and before IndigenousHost.
add_library(batbox_application_file_history
  src/application_file_history/HistoryState.cpp
  src/application_file_history/HistoryBackup.cpp
  src/application_file_history/HistorySnapshots.cpp
  src/application_file_history/HistoryRestore.cpp
  src/application_file_history/HistoryDiff.cpp
  src/application_file_history/HistoryResume.cpp)
target_link_libraries(batbox_application_file_history PUBLIC
  batbox_application_flights batbox_core batbox_event_loop OpenSSL::Crypto)
target_compile_options(batbox_application_file_history PRIVATE
  -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_gadgets PUBLIC
  batbox_application_file_history)
target_link_libraries(batbox_application_frontend PUBLIC
  batbox_application_file_history)
target_sources(batbox_application_frontend PRIVATE
  src/application_frontend/RewindModal.cpp)
