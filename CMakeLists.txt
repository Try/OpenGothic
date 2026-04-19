cmake_minimum_required(VERSION 3.16)

project(Gothic2Notr LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

add_executable(${PROJECT_NAME})

if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
endif()

if(NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wconversion -Wno-strict-aliasing -Werror)
  if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-class-memaccess)
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.1" AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-truncation)
  endif()
  if(UNIX)
    # -rdynamic is needed to allow obtaining meaningful backtraces
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    # required by pthread_setname_np()
    add_definitions(-D_GNU_SOURCE)
  endif()
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
  if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup" )
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lpthread")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lstdc++fs")
  endif()
elseif(IOS)
  enable_language(OBJCXX)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
  set_directory_properties("${CMAKE_SOURCE_DIR}" PROPERTIES
    PROPERTY XCODE_STARTUP_PROJECT "${PROJECT_NAME}")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "opengothic.gothic2"
    MACOSX_BUNDLE_BUNDLE_VERSION 1
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
elseif(APPLE)
  enable_language(OBJCXX)
endif()

if(IOS)
  file(GLOB_RECURSE ObjCSOURCES "game/*.mm")
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework AppKit")
  file(GLOB_RECURSE ObjCSOURCES "game/*.mm")
endif()

## OpenGothic

# sources
file(GLOB_RECURSE OPENGOTHIC_SOURCES
    "game/*.h"
    "game/*.cpp")
# NT5 dmsynth source files (under game/dmusic/nt5/) are added conditionally
# by the OPENGOTHIC_WITH_NT5_DMSYNTH block below; exclude them from the
# default glob so they are not pulled into the build unintentionally.
list(FILTER OPENGOTHIC_SOURCES EXCLUDE REGEX "/game/dmusic/nt5/")

target_sources(${PROJECT_NAME} PRIVATE ${OPENGOTHIC_SOURCES} ${ObjCSOURCES} icon.rc)

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} GothicShaders)

include_directories("game")

## submodule dependencies

# edd-dbg
if(WIN32)
  add_subdirectory(lib/edd-dbg)
  include_directories(lib/edd-dbg/include)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread -ldl)
endif()

# ZenKit
set(BUILD_SQUISH_WITH_SSE2     OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN             OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION      OFF CACHE INTERNAL "")

add_subdirectory(lib/ZenKit)
target_link_libraries(${PROJECT_NAME} zenkit)

# dmusic
set(DM_BUILD_STATIC            ON  CACHE INTERNAL "")
set(DM_ENABLE_ASAN             OFF CACHE INTERNAL "")

add_subdirectory(lib/dmusic)
target_link_libraries(${PROJECT_NAME} dmusic)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# NT5 dmsynth direct sources (optional)
# When ON: compiles NT5 dmsynth source files from game/dmusic/nt5/ if present.
# Place the NT5 source files there (voice.cpp, mix.cpp, mixf.cpp, etc. from
# tongzx/nt5src XPSP1/NT/multimedia/directx/dmusic/dmsynth/) and nt5compat.h
# will make them compile cross-platform without _X86_ assembly or Windows SDK.
option(OPENGOTHIC_WITH_NT5_DMSYNTH
  "Use NT5 dmsynth sources directly (place NT5 files in game/dmusic/nt5/)" OFF)
if(OPENGOTHIC_WITH_NT5_DMSYNTH)
  message(STATUS "NT5 dmsynth: direct source mode enabled")
  target_compile_definitions(${PROJECT_NAME} PRIVATE OPENGOTHIC_WITH_NT5_DMSYNTH=1)
  # Add nt5/ to include path so NT5 files can #include "nt5compat.h"
  target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game/dmusic/nt5")
  # Conditionally add NT5 source files if present
  # Note: voice.cpp / mix.cpp / mixf.cpp still depend on unported NT5 headers
  # (debug.h, misc.h, csynth.h, …) and will be re-enabled once those land.
  # sverb.cpp is self-contained and is the only process-side NT5 module we
  # need for our current reverb drop-in.
  foreach(_nt5_file sverb.cpp)
    set(_nt5_path "${CMAKE_CURRENT_SOURCE_DIR}/game/dmusic/nt5/${_nt5_file}")
    if(EXISTS "${_nt5_path}")
      target_sources(${PROJECT_NAME} PRIVATE "${_nt5_path}")
      message(STATUS "  NT5 dmsynth: adding ${_nt5_file}")
    else()
      message(STATUS "  NT5 dmsynth: ${_nt5_file} not found (skipped)")
    endif()
  endforeach()
  unset(_nt5_file)
  unset(_nt5_path)
else()
  message(STATUS "NT5 dmsynth: using portable reimplementation (default)")
endif()

# Tempest
if(WIN32)
  set(TEMPEST_BUILD_SHARED ON  CACHE INTERNAL "")
else()
  set(TEMPEST_BUILD_SHARED OFF CACHE INTERNAL "")
endif()
add_subdirectory(lib/Tempest/Engine)
include_directories(lib/Tempest/Engine/include)
target_link_libraries(${PROJECT_NAME} Tempest)

# zip
include_directories(lib/miniz)
add_library(miniz STATIC "lib/miniz/miniz.h" "lib/miniz/miniz.c")
target_link_libraries(${PROJECT_NAME} miniz)

# bullet physics
set(BULLET2_MULTITHREADING       ON  CACHE INTERNAL "")
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON  CACHE INTERNAL "")
set(USE_GRAPHICAL_BENCHMARK      OFF CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_OPENGL3_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_UNIT_TESTS             OFF CACHE INTERNAL "") # No tests
set(BUILD_CPU_DEMOS              OFF CACHE INTERNAL "") # No samples
set(BUILD_EXTRAS                 OFF CACHE INTERNAL "") # No bugs
set(GLFW_BUILD_EXAMPLES          OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS             OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS              OFF CACHE INTERNAL "")
set(BUILD_BULLET3                OFF CACHE INTERNAL "") # Can use bullet2, bullet3 wants to build examples...
add_subdirectory(lib/bullet3)
if(NOT MSVC)
  target_compile_options(BulletDynamics         PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-argument-outside-range)
  target_compile_options(Bullet3Common          PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletCollision        PRIVATE -Wno-argument-outside-range)
  target_compile_options(LinearMath             PRIVATE -Wno-argument-outside-range)
endif()
if(NOT MSVC AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  target_compile_options(BulletDynamics         PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-stringop-overflow)
  target_compile_options(Bullet3Common          PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletCollision        PRIVATE -Wno-stringop-overflow)
  target_compile_options(LinearMath             PRIVATE -Wno-stringop-overflow)
endif()

include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# script for launching in binary directory
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.bat
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.bat)
endif()

# in debug mode, enable sanitizers
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  add_compile_options(-fsanitize=address)
  add_compile_options(-fsanitize=leak)
  add_link_options   (-fsanitize=address)
  add_link_options   (-fsanitize=leak)
endif()

# installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
