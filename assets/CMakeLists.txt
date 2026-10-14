cmake_minimum_required(VERSION 3.12)

# Diathic (gothic-diablo): our own files built into the game (menu logo, menu music).
# Every file of assets/diathic becomes DiathicAsset::get("<name>"); a changed or new file reconfigures CMake.

project(DiathicAssets LANGUAGES CXX)

file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/diathic/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCES})

set(HEADER "${PROJECT_BINARY_DIR}/diathicassets.h")
set(CPP    "${PROJECT_BINARY_DIR}/diathicassets.cpp")

file(WRITE ${CPP}
  "#include \"diathicassets.h\"\n\n")

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(MAKE_C_IDENTIFIER ${NAME} CLEAN_NAME)
  file(READ "${i}" ASSET_FILE HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ASSET_FILE ${ASSET_FILE})
  file(APPEND ${CPP} "alignas(uint32_t) static const uint8_t DATA_${CLEAN_NAME}[] = {${ASSET_FILE}};\n")
endforeach()
file(APPEND ${CPP} "\n")

file(APPEND ${CPP} "DiathicAsset DiathicAsset::get(std::string_view name) {\n")
foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(MAKE_C_IDENTIFIER ${NAME} CLEAN_NAME)
  file(APPEND ${CPP} "  if(name==\"${NAME}\")\n")
  file(APPEND ${CPP} "    return {reinterpret_cast<const char*>(DATA_${CLEAN_NAME}),sizeof(DATA_${CLEAN_NAME})};\n")
endforeach()
file(APPEND ${CPP} "  return {nullptr,0};\n")
file(APPEND ${CPP} "  }\n")

file(WRITE ${HEADER}
  "#pragma once\n"
  "#include <cstddef>\n"
  "#include <cstdint>\n"
  "#include <string_view>\n"
  "\n"
  "struct DiathicAsset {\n"
  "  const char* data = nullptr;\n"
  "  size_t      len  = 0;\n"
  "  bool        empty() const { return len==0; }\n"
  "  static DiathicAsset get(std::string_view name); // e.g. \"logo.png\"; empty when there is no such file\n"
  "  };\n")

add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/")
