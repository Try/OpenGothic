cmake_minimum_required(VERSION 2.8)

project(Gothic2Notr)
set (CMAKE_CXX_STANDARD 14)

include_directories("Game")

set(CMAKE_BINARY_DIR ../)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB SOURCES
    "Game/*.h"
    "Game/*.cpp"
    "Game/**/*.h"
    "Game/**/*.cpp"
    "Game/**/**/*.h"
    "Game/**/**/*.cpp"
    "Game/**/**/**/*.h"
    "Game/**/**/**/*.cpp"
  )

link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/Lib32")

add_executable(${PROJECT_NAME} ${SOURCES} )

add_subdirectory(lib/ZenLib)
include_directories(lib/ZenLib)
target_link_libraries(${PROJECT_NAME} zenload daedalus)

add_subdirectory(lib/MoltenTempest/Engine)
include_directories(lib/MoltenTempest/Engine/include)
target_link_libraries(${PROJECT_NAME} MoltenTempest)
