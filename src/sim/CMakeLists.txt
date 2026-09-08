add_library(
  romulus_sim
  src/city_map.cpp
  src/road_pathfinder.cpp
  src/city_simulation.cpp
)
add_library(romulus::sim ALIAS romulus_sim)

target_include_directories(
  romulus_sim
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
