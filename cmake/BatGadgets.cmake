target_sources(batbox_bats PRIVATE
  src/bats/BatResultBindings.cpp
  src/bats/BatGadgetCall.cpp src/bats/BatGadgetLaunch.cpp
  src/bats/BatGadgetPoetry.cpp src/bats/BatGadgetAsync.cpp
  src/bats/BatGadgetForeground.cpp src/bats/BatGadgetForegroundProgress.cpp
  src/bats/BatGadgetForegroundFinalize.cpp src/bats/BatGadgetBackgroundContinuation.cpp
  src/bats/BatGadgetStream.cpp src/bats/BatGadgetRace.cpp
  src/bats/BatGadgetBinding.cpp src/bats/BatGadgetDescriptor.cpp
  src/bats/BatGadgetPoetryDescriptor.cpp src/bats/BatGadgetSchema.cpp)
target_link_libraries(batbox_bats PUBLIC batbox_event_loop)
