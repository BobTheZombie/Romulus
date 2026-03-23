cmake_minimum_required(VERSION 3.20)

project(Romulus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)

add_subdirectory(src/core)

add_library(romulus_platform INTERFACE)
add_library(romulus::platform ALIAS romulus_platform)
target_include_directories(romulus_platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(romulus_render INTERFACE)
add_library(romulus::render ALIAS romulus_render)
target_include_directories(romulus_render INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(romulus_data INTERFACE)
add_library(romulus::data ALIAS romulus_data)
target_include_directories(romulus_data INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
    romulus::core
    SDL2::SDL2
)
target_include_directories(caesar2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include)
