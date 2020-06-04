cmake_minimum_required(VERSION 3.12)

project(Gothic2Notr LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "Build shared libraries." OFF)

if(POLICY CMP0990)
  cmake_policy(SET CMP0990 OLD)
endif()

if(POLICY CMP0015)
  cmake_policy(SET CMP0015 OLD)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-DNOMINMAX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
endif()

## submodule dependencies

# edd-dbg
add_subdirectory(lib/edd-dbg)

# ZenLib
add_subdirectory(lib/ZenLib)

# bullet physics
set(BULLET2_MULTITHREADING ON)
set(USE_GRAPHICAL_BENCHMARK OFF CACHE STRING "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON)
set(BUILD_BULLET2_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_OPENGL3_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_UNIT_TESTS OFF CACHE STRING "" FORCE) # No tests
set(BUILD_CPU_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_EXTRAS OFF CACHE STRING "" FORCE) # No bugs
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE STRING "" FORCE)
set(BUILD_BULLET3 OFF CACHE STRING "" FORCE) # Can use bullet2, bullet3 wants to build examples...
set(BULLET2_USE_OPEN_MP_MULTITHREADING OFF CACHE STRING "" FORCE)
set(BULLET2_USE_PPL_MULTITHREADING OFF CACHE STRING "" FORCE)
set(BULLET2_USE_TBB_MULTITHREADING OFF CACHE STRING "" FORCE)

add_subdirectory(lib/bullet3)

# MoltenTempest
set(TEMPEST_BUILD_SHARED ON)
add_subdirectory(lib/MoltenTempest/Engine)

## OpenGothic

# sources
file(GLOB OPENGOTHIC_SOURCES
    "Game/*.h"
    "Game/*.cpp"
    "Game/**/*.h"
    "Game/**/*.cpp"
    "Game/**/**/*.h"
    "Game/**/**/*.cpp"
    "Game/**/**/**/*.h"
    "Game/**/**/**/*.cpp")

# shaders
add_subdirectory(shader)

# executable
add_executable(${PROJECT_NAME}
    ${OPENGOTHIC_SOURCES}
    icon.rc)

include_directories("Game")

# edd-dbg
include_directories(lib/edd-dbg/include)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread)
endif()

# ZenLib
include_directories(lib/ZenLib)
target_link_libraries(${PROJECT_NAME} zenload daedalus)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# bullet physics
include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# MoltenTempest
include_directories(lib/MoltenTempest/Engine/include)
target_link_libraries(${PROJECT_NAME} MoltenTempest)

# shaders
target_link_libraries(${PROJECT_NAME} GothicShaders)

if(NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wconversion -Wno-strict-aliasing -Werror)
endif()

# script for launching in binary directory
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.bat
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.bat)
elseif(UNIX)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.sh
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.sh)
endif()

# installation
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
    )
