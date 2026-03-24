cmake_minimum_required(VERSION 3.20)

project(Romulus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

find_package(SDL2 REQUIRED)

add_subdirectory(src/core)
add_subdirectory(src/data)
add_subdirectory(src/platform)

add_library(romulus_render INTERFACE)
add_library(romulus::render ALIAS romulus_render)
target_include_directories(romulus_render INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(romulus_sim INTERFACE)
add_library(romulus::sim ALIAS romulus_sim)
target_include_directories(romulus_sim INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(romulus_ui INTERFACE)
add_library(romulus::ui ALIAS romulus_ui)
target_include_directories(romulus_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(caesar2 apps/caesar2/main.cpp)
target_link_libraries(
  caesar2
  PRIVATE
    romulus::platform
)

add_subdirectory(tests)
