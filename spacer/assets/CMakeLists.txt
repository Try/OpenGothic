cmake_minimum_required(VERSION 3.12)

project(SpacerAssets LANGUAGES CXX)

file(GLOB SOURCES "**/*.png")

set(HEADER "${PROJECT_BINARY_DIR}/assetfile.h")
set(CPP    "${PROJECT_BINARY_DIR}/assetfile.cpp")

file(WRITE ${CPP}
  "#include \"assetfile.h\"\n\n")

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(READ "${i}" ASSET_FILE HEX)
  string(LENGTH ${ASSET_FILE} ASSET_FILE_LEN)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ASSET_FILE ${ASSET_FILE})

  file(APPEND ${CPP} "alignas(uint32_t) static const uint8_t DATA_${CLEAN_NAME}[] = {${ASSET_FILE}};\n")
endforeach()
file(APPEND ${CPP} "\n")

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${CPP} "const AssetFile AssetFile::${CLEAN_NAME} = {")
  file(APPEND ${CPP} "reinterpret_cast<const char*>(DATA_${CLEAN_NAME}),sizeof(DATA_${CLEAN_NAME})")
  file(APPEND ${CPP} "};\n")
endforeach()
file(APPEND ${CPP} "\n")


file(WRITE ${HEADER}
  "#pragma once\n"
  "#include <cstdint>\n"
  "#include <string_view>\n"
  "#include <string>\n"
  "#include <stdexcept>\n"
  "#include <Tempest/Pixmap>\n"
  "#include <Tempest/MemReader>\n"
  "\n"
  "struct AssetFile {\n"
  "  const char* data;\n"
  "  size_t      len;\n"
)

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${HEADER} "  static const AssetFile ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  static AssetFile get(std::string_view name);\n")
file(APPEND ${HEADER} "  static Tempest::Pixmap pixmap(std::string_view name);\n")
file(APPEND ${HEADER} "  };\n")

file(APPEND ${HEADER} "inline AssetFile AssetFile::get(std::string_view name) {\n")
foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${HEADER} "  if(\"${NAME}\"==name)\n")
  file(APPEND ${HEADER} "    return ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  throw std::runtime_error(\"asset not found: \"+std::string(name));\n")
file(APPEND ${HEADER} "  }\n")
file(APPEND ${HEADER} "inline Tempest::Pixmap AssetFile::pixmap(std::string_view name) {\n")
file(APPEND ${HEADER} "  auto data = get(name);\n")
file(APPEND ${HEADER} "  Tempest::MemReader rd(data.data, data.len);\n")
file(APPEND ${HEADER} "  return Tempest::Pixmap(rd);\n")
file(APPEND ${HEADER} "  }\n")


add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/lib/Tempest/Engine/include")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${HEADER})