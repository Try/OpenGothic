cmake_minimum_required(VERSION 2.4)
project(edd_dbg)

set(CMAKE_CXX_STANDARD 14)

file(GLOB SOURCES
    "src/*.h"
    "src/*.cpp")

if(MINGW)
  file(GLOB SOURCES_WINDOWS
      "src/windows/*.hpp"
      "src/windows/*.cpp"
      "src/windows/mingw/*.hpp"
      "src/windows/mingw/*.cpp")
elseif(MSVC)
  file(GLOB SOURCES_WINDOWS
      "src/windows/*.hpp"
      "src/windows/*.cpp"
      "src/windows/msvc/*.hpp"
      "src/windows/msvc/*.cpp")
endif(MSVC)

file(GLOB SOURCES_OSX
    "src/osx/*.hpp"
    "src/osx/*.cpp")

add_definitions("-DDBG_DONT_USE_FUNGO")

if(APPLE)
  add_library(edd_dbg STATIC ${SOURCES} ${SOURCES_OSX})
elseif(WIN32)
  add_library(edd_dbg STATIC ${SOURCES} ${SOURCES_WINDOWS})
endif(APPLE)


include_directories(include)
include_directories(src)
include_directories(src/windows)

