cmake_minimum_required(VERSION 3.12)

project(GothicShaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/*.tesc"
  "${CMAKE_SOURCE_DIR}/shader/*.tese"
  "${CMAKE_SOURCE_DIR}/shader/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/*.glsl"
  "${CMAKE_SOURCE_DIR}/shader/**/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/**/*.tesc"
  "${CMAKE_SOURCE_DIR}/shader/**/*.tese"
  "${CMAKE_SOURCE_DIR}/shader/**/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/**/*.glsl")

file(GLOB GLSL_SOURCES "${CMAKE_SOURCE_DIR}/shader/*.glsl")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator "/opt/homebrew/bin")
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(add_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${PROJECT_BINARY_DIR}/sprv/${OUTPUT}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/shader/${SOURCE}")
  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      ${SOURCE_FILE} PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} ${OUTPUT_FILE} PARENT_SCOPE)

  list(SUBLIST ARGV 2 -1 VARS)
  add_custom_command(
    OUTPUT     ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE} ${GLSL_SOURCES}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

# Ubershader flags:
#   OBJ        - static object
#   SKINING    - skeleton animation
#   MORPH      - morphing animation
#   VCOLOR     - enable vertex color
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation
#   WATER      - water material
#   MORPH      - morphing animation
#   G1         - hint for gothic1 shader
function(add_uber_shader OUTPUT)
  list(SUBLIST ARGV 1 -1 VARS)
  add_shader(${OUTPUT}_shadow.vert      main.vert ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.frag      main.frag ${VARS} -DSHADOW_MAP)

  add_shader(${OUTPUT}_shadow_at.vert   main.vert ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.frag   main.frag ${VARS} -DSHADOW_MAP -DATEST)

  add_shader(${OUTPUT}.vert             main.vert ${VARS} -DLIGHT)
  add_shader(${OUTPUT}.frag             main.frag ${VARS} -DLIGHT)

  add_shader(${OUTPUT}_at.vert          main.vert ${VARS} -DLIGHT -DATEST)
  add_shader(${OUTPUT}_at.frag          main.frag ${VARS} -DLIGHT -DATEST)

  add_shader(${OUTPUT}_emi.vert         main.vert ${VARS})
  add_shader(${OUTPUT}_emi.frag         main.frag ${VARS})

  add_shader(${OUTPUT}_gbuffer.vert     main.vert ${VARS} -DLIGHT -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.frag     main.frag ${VARS} -DLIGHT -DGBUFFER)

  add_shader(${OUTPUT}_gbuffer_at.vert  main.vert ${VARS} -DLIGHT -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.frag  main.frag ${VARS} -DLIGHT -DGBUFFER -DATEST)

  add_shader(${OUTPUT}_water.vert       main.vert ${VARS} -DLIGHT -DWATER)
  add_shader(${OUTPUT}_water.tesc       main.tesc ${VARS} -DLIGHT -DWATER)
  add_shader(${OUTPUT}_water.tese       main.tese ${VARS} -DLIGHT -DWATER)
  add_shader(${OUTPUT}_water.frag       main.frag ${VARS} -DLIGHT -DWATER)

  add_shader(${OUTPUT}_ghost.vert       main.vert ${VARS} -DLIGHT -DGHOST)
  add_shader(${OUTPUT}_ghost.frag       main.frag ${VARS} -DLIGHT -DGHOST)

  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} PARENT_SCOPE)
endfunction(add_uber_shader)

# shaders
add_uber_shader(clr -DVCOLOR)
add_uber_shader(obj -DOBJ)
add_uber_shader(ani -DSKINING)
add_uber_shader(mph -DMORPH)

add_shader(light.vert           light.vert "")
add_shader(light.frag           light.frag "")

# Old nishita sky model
add_shader(fog.vert             sky/sky.vert         -DFOG)
add_shader(fog.frag             sky/sky_nishita.frag -DFOG)
add_shader(sky_g2.vert          sky/sky.vert         -DG2)
add_shader(sky_g2.frag          sky/sky_nishita.frag -DG2)
add_shader(sky_g1.vert          sky/sky.vert         -DG1)
add_shader(sky_g1.frag          sky/sky_nishita.frag -DG1)

# Based on Scalable and Production Ready Sky and Atmosphere
# https://sebh.github.io/publications/egsr2020.pdf
# https://www.shadertoy.com/view/slSXRW
add_shader(sky_transmittance.vert    sky/sky.vert                  "")
add_shader(sky_transmittance.frag    sky/sky_transmittance.frag    "")
add_shader(sky_multi_scattering.vert sky/sky.vert                  "")
add_shader(sky_multi_scattering.frag sky/sky_multi_scattering.frag "")
add_shader(sky_view_lut.vert         sky/sky.vert                  "")
add_shader(sky_view_lut.frag         sky/sky_view_lut.frag         "")

add_shader(fog_prsr.vert             sky/sky.vert         -DFOG2)
add_shader(fog_prsr.frag             sky/sky_prsr.frag    -DFOG2)
add_shader(sky_prsr_g2.vert          sky/sky.vert         -DG2)
add_shader(sky_prsr_g2.frag          sky/sky_prsr.frag    -DG2)

add_shader(copy.vert            copy.vert "")
add_shader(copy.frag            copy.frag "")

add_shader(ssao.vert            ssao.vert "")
add_shader(ssao.frag            ssao.frag "")

add_shader(bilateral.vert       copy.vert "")
add_shader(bilateral.frag       bilateral.frag "")

add_shader(ssao_compose.vert    copy.vert "")
add_shader(ssao_compose.frag    ssao_compose.frag "")

add_custom_command(
  OUTPUT     ${HEADER} ${CPP}
  DEPENDS    ${OPENGOTHIC_SHADERS_SPRV}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
  COMMAND    ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/shader/link_shaders.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(shader
  DEPENDS ${HEADER} ${CPP}
  SOURCES ${SOURCES} "${CMAKE_SOURCE_DIR}/shader/link_shaders.cmake")

add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
add_dependencies(${PROJECT_NAME} shader)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/sprv")
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER ${HEADER}
    )
