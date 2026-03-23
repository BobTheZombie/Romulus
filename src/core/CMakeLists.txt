add_library(romulus_core src/logger.cpp)
add_library(romulus::core ALIAS romulus_core)

target_include_directories(
  romulus_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
