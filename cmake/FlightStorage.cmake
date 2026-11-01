add_library(batbox_flights
  src/flights/AsyncStash.cpp
  src/flights/ProjectDirectories.cpp
  src/flights/ProjectDiscovery.cpp src/flights/ProjectDiscoveryIndigenous.cpp
  src/flights/LoadParsing.cpp src/flights/AsyncLoad.cpp src/flights/AsyncLoadIndigenous.cpp
  src/flights/BatTranscript.cpp src/flights/AsyncBatTranscript.cpp
  src/flights/Cleaning.cpp src/flights/CleaningRefs.cpp src/flights/PoetryEntries.cpp
  src/flights/Records.cpp src/flights/RecordsRefs.cpp
  src/flights/Graph.cpp src/flights/Relink.cpp src/flights/Paths.cpp
  src/flights/Load.cpp src/flights/WriteQueue.cpp src/flights/QueueIO.cpp
  src/flights/Metadata.cpp src/flights/Project.cpp src/flights/ProjectMetadata.cpp
  src/flights/Stash.cpp src/flights/Tombstone.cpp src/flights/Views.cpp
  src/flights/MetadataUpdates.cpp src/flights/MetadataStash.cpp
  src/flights/FlightListing.cpp)
target_sources(batbox_flights PRIVATE
  src/flights/scanning/BoundaryCore.cpp src/flights/scanning/BoundaryReader.cpp
  src/flights/scanning/ChainFilter.cpp src/flights/scanning/MetadataScan.cpp)
target_sources(batbox_flights PRIVATE src/flights/async_queue/Queue.cpp src/flights/async_queue/AsyncFileSystem.cpp)
target_sources(batbox_flights PRIVATE src/flights/SavedBats.cpp)
target_sources(batbox_flights PRIVATE
  src/flights/async_project/AsyncProject.cpp src/flights/async_project/MessageChain.cpp
  src/flights/async_project/Records.cpp src/flights/async_project/IndigenousStat.cpp)
target_link_libraries(batbox_flights PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_flights PRIVATE -Wall -Wextra -Wpedantic)
