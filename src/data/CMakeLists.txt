add_library(romulus_data
  src/data_root.cpp
  src/file_inventory.cpp
  src/binary_reader.cpp
  src/file_loader.cpp
  src/binary_probe.cpp
)
add_library(romulus::data ALIAS romulus_data)

target_include_directories(
  romulus_data
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
