add_library(romulus_platform src/application.cpp src/bootstrap.cpp src/viewer.cpp src/startup.cpp)
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
    romulus::data
    SDL2::SDL2
)
