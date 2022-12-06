file(GLOB SOURCES
    "sprv/*.sprv"
  )

set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${CMAKE_CURRENT_BINARY_DIR}/sprv/shader.cpp")

file(WRITE ${HEADER}
  "#include <cstdint>\n"
  "#include <string_view>\n"
  "#include <stdexcept>\n"
  "\n"
  "struct GothicShader {\n"
  "  const char* data;\n"
  "  size_t      len;\n"
)


foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${HEADER} "  static const GothicShader ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  static GothicShader get(std::string_view name);\n")
file(APPEND ${HEADER} "  };\n")

file(APPEND ${HEADER} "inline GothicShader GothicShader::get(std::string_view name) {\n")
foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${HEADER} "  if(\"${NAME}\"==name)\n")
  file(APPEND ${HEADER} "    return ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  throw std::runtime_error(\"\");\n")
file(APPEND ${HEADER} "  }\n")

file(WRITE ${CPP}
  "#include \"shader.h\"\n")

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(READ "${i}" SHADER_SPRV HEX)
  string(LENGTH ${SHADER_SPRV} SHADER_SPRV_LEN)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," SHADER_SPRV ${SHADER_SPRV})

  file(APPEND ${CPP} "static const uint8_t SRC_${CLEAN_NAME}[] = {${SHADER_SPRV}};\n")
endforeach()
file(APPEND ${CPP} "\n")

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(APPEND ${CPP} "const GothicShader GothicShader::${CLEAN_NAME} = {")
  file(APPEND ${CPP} "reinterpret_cast<const char*>(SRC_${CLEAN_NAME}),sizeof(SRC_${CLEAN_NAME})")
  file(APPEND ${CPP} "};\n")
endforeach()
file(APPEND ${CPP} "\n")

