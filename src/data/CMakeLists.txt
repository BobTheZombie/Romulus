add_library(romulus_data src/data_root.cpp)
add_library(romulus::data ALIAS romulus_data)

target_include_directories(
  romulus_data
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
