add_library(romulus_platform src/application.cpp)
add_library(romulus::platform ALIAS romulus_platform)

target_include_directories(
  romulus_platform
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
  romulus_platform
  PUBLIC
    romulus::core
    SDL2::SDL2
)
