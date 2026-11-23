add_library(batbox_history src/history/References.cpp src/history/PastedContent.cpp
  src/history/Reader.cpp src/history/ReaderIndigenous.cpp)
target_link_libraries(batbox_history PUBLIC batbox_core batbox_event_loop batbox_fs_operations)
target_compile_options(batbox_history PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_flights PUBLIC batbox_fs_operations batbox_cleanup)
target_sources(batbox_history PRIVATE src/history/ViewBasic.cpp
  src/history/ViewsIndigenous.cpp)
target_sources(batbox_history PRIVATE src/history/WriterFlush.cpp src/history/WriterPrepare.cpp
  src/history/WriterPublic.cpp src/history/WriterIndigenous.cpp)
target_link_libraries(batbox_history PUBLIC batbox_locking batbox_cleanup OpenSSL::Crypto)
# The paste store (formerly batbox_paste_store, sole consumer batbox_history)
# is part of the history mechanism: pasted-content blobs beside the history
# trail.
target_sources(batbox_history PRIVATE
  src/paste_store/PathsHash.cpp src/paste_store/StoreRetrieve.cpp
  src/paste_store/Cleanup.cpp src/paste_store/Indigenous.cpp)
