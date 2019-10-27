cmake_minimum_required(VERSION 2.8)

project(Gothic2Notr)
set (CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "Build shared libraries." OFF)
option(BUILD_SHARED_MOLTEN_TEMPEST "Build shared MoltenTempest." ${BUILD_SHARED_LIBS})

set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(POLICY CMP0990)
  cmake_policy(SET CMP0990 OLD)
endif()

if(POLICY CMP0015)
  cmake_policy(SET CMP0015 OLD)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_SKIP_RPATH ON)


# OpenGothic
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

add_executable(${PROJECT_NAME} ${SOURCES} icon.rc)

include_directories("Game")

add_subdirectory(lib/edd-dbg)
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
add_subdirectory(lib/ZenLib)
include_directories(lib/ZenLib)
target_link_libraries(${PROJECT_NAME} zenload daedalus)


# bullet physics
set(BULLET2_MULTITHREADING ON)
set(USE_GRAPHICAL_BENCHMARK OFF CACHE STRING "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_OPENGL3_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_UNIT_TESTS OFF CACHE STRING "" FORCE) # No tests
set(BUILD_CPU_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_EXTRAS OFF CACHE STRING "" FORCE) # No bugs
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE STRING "" FORCE)
set(BUILD_BULLET3 OFF CACHE STRING "" FORCE) # Can use bullet2, bullet3 wants to build examples...

add_subdirectory(lib/bullet3)

include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)


# MoltenTempest
set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory(lib/MoltenTempest/Engine)

include_directories(lib/MoltenTempest/Engine/include)
target_link_libraries(${PROJECT_NAME} MoltenTempest)


# script for launching in build directory
if(UNIX)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.sh
        ${CMAKE_CURRENT_BINARY_DIR}/bin/Gothic2Notr.sh)
endif()


# installation
install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    )
