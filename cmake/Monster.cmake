add_library(batbox_monster src/monster/Random.cpp src/monster/Roller.cpp
  src/monster/Tables.cpp src/monster/Poetry.cpp src/monster/Triggers.cpp)
target_link_libraries(batbox_monster PUBLIC batbox_core)
target_compile_options(batbox_monster PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_monster PRIVATE src/monster/Sprites.cpp
  src/monster/SpriteData.cpp src/monster/Geometry.cpp)
