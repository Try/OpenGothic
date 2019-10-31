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

if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
elseif(UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic/bin)
endif()
set(CMAKE_SKIP_RPATH ON)


## system dependencies

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator required")
endif()

function(compile_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/opengothic/shader/${OUTPUT}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/shader/${SOURCE}")
  list(SUBLIST ARGV 2 -1 VARS)

  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/opengothic/shader/"
    COMMAND ${GLSLANGVALIDATOR} -V ${VARS} ${SOURCE_FILE} -o ${OUTPUT_FILE}
    DEPENDS ${SOURCE_FILE})
  set(OPENGOTHIC_SHADERS ${OPENGOTHIC_SHADERS} ${OUTPUT_FILE} PARENT_SCOPE)
endfunction(compile_shader)


## submodule dependencies

# edd-dbg
add_subdirectory(lib/edd-dbg)

# ZenLib
add_subdirectory(lib/ZenLib)

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

# MoltenTempest
set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory(lib/MoltenTempest/Engine)


## OpenGothic

# sources
# OpenGothic
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
# Ubershader flags:
#   OBJ        - enable object matrix
#   SKINING    - animation skeleton
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation
compile_shader(land.vert            main.vert -DATEST)
compile_shader(land.frag            main.frag -DATEST)
compile_shader(object.vert          main.vert -DATEST -DOBJ)
compile_shader(object.frag          main.frag -DATEST -DOBJ)
compile_shader(anim.vert            main.vert -DATEST -DOBJ -DSKINING)
compile_shader(anim.frag            main.frag -DATEST -DOBJ -DSKINING)
compile_shader(pfx.vert             main.vert -DPFX)
compile_shader(pfx.frag             main.frag -DPFX)
compile_shader(land_shadow.vert     main.vert -DATEST -DSHADOW_MAP)
compile_shader(land_shadow.frag     main.frag -DATEST -DSHADOW_MAP)
compile_shader(object_shadow.vert   main.vert -DATEST -DOBJ -DSHADOW_MAP)
compile_shader(object_shadow.frag   main.frag -DATEST -DOBJ -DSHADOW_MAP)
compile_shader(anim_shadow.vert     main.vert -DOBJ -DSKINING -DSHADOW_MAP)
compile_shader(anim_shadow.frag     main.frag -DOBJ -DSKINING -DSHADOW_MAP)
compile_shader(pfx_shadow.vert      main.vert -DSHADOW_MAP)
compile_shader(pfx_shadow.frag      main.frag -DSHADOW_MAP)
compile_shader(sky.vert             sky.vert "")
compile_shader(sky.frag             sky.frag "")
compile_shader(shadow_compose.vert  shadow_compose.vert "")
compile_shader(shadow_compose.frag  shadow_compose.frag "")

# executable
add_executable(${PROJECT_NAME}
    ${OPENGOTHIC_SOURCES}
    ${OPENGOTHIC_SHADERS}
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

# bullet physics
include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# MoltenTempest
include_directories(lib/MoltenTempest/Engine/include)
target_link_libraries(${PROJECT_NAME} MoltenTempest)

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data/
    ${CMAKE_CURRENT_BINARY_DIR}/opengothic/data/)

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
# FIXME: install data and shaders
