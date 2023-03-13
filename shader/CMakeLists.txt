cmake_minimum_required(VERSION 3.12)

project(GothicShaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB_RECURSE SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/*.tesc"
  "${CMAKE_SOURCE_DIR}/shader/*.tese"
  "${CMAKE_SOURCE_DIR}/shader/*.geom"
  "${CMAKE_SOURCE_DIR}/shader/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/*.glsl"
  "${CMAKE_SOURCE_DIR}/shader/*.task"
  "${CMAKE_SOURCE_DIR}/shader/*.mesh"
  "${CMAKE_SOURCE_DIR}/shader/*.comp")

file(GLOB_RECURSE GLSL_SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.glsl")

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

  if("${OUTPUT}" MATCHES "\.(mesh)")
    list(APPEND VARS "--target-env" "spirv1.4")
  endif()

  if("${OUTPUT}" MATCHES "\.(task)")
    list(APPEND VARS "--target-env" "spirv1.4")
  endif()

  if("${OUTPUT}" MATCHES "\.(16).")
    list(APPEND VARS "-DWORKGROUP_SIZE=16")
  endif()

  if("${OUTPUT}" MATCHES "\.(32).")
    list(APPEND VARS "-DWORKGROUP_SIZE=32")
  endif()

  if("${OUTPUT}" MATCHES "\.(64).")
    list(APPEND VARS "-DWORKGROUP_SIZE=64")
  endif()

  add_custom_command(
    OUTPUT     ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE} ${GLSL_SOURCES}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

# Ubershader flags:
#   MESH_TYPE  - type of mesh enum{landscape, object, skinned, morph, pfx}
#   SHADOW_MAP - output is shadowmap
#   FORWARD    - compute forward lighting
#   GBUFFER    - emit gbuffer
#   ATEST      - use alpha test
#   WATER      - water material
#   GHOST      - ghost material
#   G1         - hint for gothic1 shader
function(add_uber_shader OUTPUT)
  list(SUBLIST ARGV 1 -1 VARS)
  add_shader(${OUTPUT}_shadow.vert           materials/main.vert ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.32.mesh        materials/main.mesh ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.64.mesh        materials/main.mesh ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.32.task        materials/main.task ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.64.task        materials/main.task ${VARS} -DSHADOW_MAP)
  add_shader(${OUTPUT}_shadow.frag           materials/main.frag ${VARS} -DSHADOW_MAP)

  add_shader(${OUTPUT}_shadow_at.vert        materials/main.vert ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.32.mesh     materials/main.mesh ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.64.mesh     materials/main.mesh ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.32.task     materials/main.task ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.64.task     materials/main.task ${VARS} -DSHADOW_MAP -DATEST)
  add_shader(${OUTPUT}_shadow_at.frag        materials/main.frag ${VARS} -DSHADOW_MAP -DATEST)

  add_shader(${OUTPUT}.vert                  materials/main.vert ${VARS} -DFORWARD)
  add_shader(${OUTPUT}.32.mesh               materials/main.mesh ${VARS} -DFORWARD)
  add_shader(${OUTPUT}.64.mesh               materials/main.mesh ${VARS} -DFORWARD)
  add_shader(${OUTPUT}.32.task               materials/main.task ${VARS} -DFORWARD)
  add_shader(${OUTPUT}.64.task               materials/main.task ${VARS} -DFORWARD)
  add_shader(${OUTPUT}.frag                  materials/main.frag ${VARS} -DFORWARD)

  add_shader(${OUTPUT}_at.vert               materials/main.vert ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.32.mesh            materials/main.mesh ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.64.mesh            materials/main.mesh ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.32.task            materials/main.task ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.64.task            materials/main.task ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.frag               materials/main.frag ${VARS} -DFORWARD -DATEST)

  add_shader(${OUTPUT}_emi.vert              materials/main.vert ${VARS})
  add_shader(${OUTPUT}_emi.32.mesh           materials/main.mesh ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_emi.64.mesh           materials/main.mesh ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_emi.32.task           materials/main.task ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_emi.64.task           materials/main.task ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_emi.frag              materials/main.frag ${VARS} -DEMISSIVE)

  add_shader(${OUTPUT}_gbuffer.vert          materials/main.vert ${VARS} -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.32.mesh       materials/main.mesh ${VARS} -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.64.mesh       materials/main.mesh ${VARS} -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.32.task       materials/main.task ${VARS} -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.64.task       materials/main.task ${VARS} -DGBUFFER)
  add_shader(${OUTPUT}_gbuffer.frag          materials/main.frag ${VARS} -DGBUFFER)

  add_shader(${OUTPUT}_gbuffer_at.vert       materials/main.vert ${VARS} -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.32.mesh    materials/main.mesh ${VARS} -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.64.mesh    materials/main.mesh ${VARS} -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.32.task    materials/main.task ${VARS} -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.64.task    materials/main.task ${VARS} -DGBUFFER -DATEST)
  add_shader(${OUTPUT}_gbuffer_at.frag       materials/main.frag ${VARS} -DGBUFFER -DATEST)

  add_shader(${OUTPUT}_water.vert            materials/main.vert ${VARS} -DFORWARD -DWATER)
  add_shader(${OUTPUT}_water.tesc            materials/main.tesc ${VARS} -DFORWARD -DWATER)
  add_shader(${OUTPUT}_water.tese            materials/main.tese ${VARS} -DFORWARD -DWATER)
  add_shader(${OUTPUT}_water.frag            materials/main.frag ${VARS} -DFORWARD -DWATER)

  add_shader(${OUTPUT}_ghost.vert            materials/main.vert ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.32.mesh         materials/main.mesh ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.64.mesh         materials/main.mesh ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.32.task         materials/main.task ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.64.task         materials/main.task ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.frag            materials/main.frag ${VARS} -DFORWARD -DGHOST)

  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} PARENT_SCOPE)
endfunction(add_uber_shader)

# shaders
add_uber_shader(lnd "-DMESH_TYPE=0")
add_uber_shader(obj "-DMESH_TYPE=1")
add_uber_shader(ani "-DMESH_TYPE=2")
add_uber_shader(mph "-DMESH_TYPE=3")
add_uber_shader(pfx "-DMESH_TYPE=4")

add_shader(item.vert                 inventory/item.vert "")
add_shader(item.frag                 inventory/item.frag "")

add_shader(lnd_hiz.32.mesh           materials/main.mesh -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.64.mesh           materials/main.mesh -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.32.task           materials/main.task -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.64.task           materials/main.task -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.frag              materials/main.frag -DMESH_TYPE=0 -DHIZ)

add_shader(shadow_resolve.vert       copy.vert "")
add_shader(shadow_resolve.frag       shadow_resolve.frag "")
add_shader(shadow_resolve_sh.frag    shadow_resolve.frag -DSHADOW_MAP)
add_shader(shadow_resolve_rq.frag    shadow_resolve.frag -DSHADOW_MAP -DRAY_QUERY -DRAY_QUERY_AT)

add_shader(light.vert                lighting/light.vert "")
add_shader(light.frag                lighting/light.frag "")
add_shader(light_rq.frag             lighting/light.frag -DRAY_QUERY)
add_shader(light_rq_at.frag          lighting/light.frag -DRAY_QUERY -DRAY_QUERY_AT)

# Based on Scalable and Production Ready Sky and Atmosphere
# https://sebh.github.io/publications/egsr2020.pdf
# https://www.shadertoy.com/view/slSXRW
add_shader(sky_transmittance.vert    sky/sky.vert                  "")
add_shader(sky_transmittance.frag    sky/sky_transmittance.frag    "")
add_shader(sky_multi_scattering.vert sky/sky.vert                  "")
add_shader(sky_multi_scattering.frag sky/sky_multi_scattering.frag "")

add_shader(sky_view_lut.vert         sky/sky.vert                  "")
add_shader(sky_view_lut.frag         sky/sky_view_lut.frag         "")
add_shader(fog_view_lut.vert         sky/sky.vert                  -DFOG)

add_shader(shadow_downsample.comp    sky/shadow_downsample.comp    "")
add_shader(fog_view_lut.frag         sky/fog_view_lut.comp         -S frag -DFOG -DFRAG)
add_shader(fog_view_lut.comp         sky/fog_view_lut.comp         -S comp -DFOG -DCOMPUTE)
add_shader(clouds_lut.comp           sky/clouds_lut.comp           "")

add_shader(fog3d.vert                sky/sky.vert         -DFOG -DVOLUMETRIC)
add_shader(fog3d.frag                sky/sky_egsr.frag    -DFOG -DVOLUMETRIC)
add_shader(sky3d.vert                sky/sky.vert         -DVOLUMETRIC)
add_shader(sky3d.frag                sky/sky_egsr.frag    -DVOLUMETRIC)

add_shader(fog.vert                  sky/sky.vert         -DFOG)
add_shader(fog.frag                  sky/sky_egsr.frag    -DFOG)
add_shader(sky.vert                  sky/sky.vert         "")
add_shader(sky.frag                  sky/sky_egsr.frag    "")
add_shader(sun.vert                  sky/sun.vert         "")
add_shader(sun.frag                  sky/sun.frag         "")

add_shader(copy.vert                 copy.vert "")
add_shader(copy.frag                 copy.frag "")

add_shader(ssao.comp                 ssao/ssao.comp "")
add_shader(ssao_rq.comp              ssao/ssao.comp -DRAY_QUERY)

add_shader(tonemapping.vert          copy.vert "")
add_shader(tonemapping.frag          tonemapping.frag "")

add_shader(ssao_compose.vert         copy.vert "")
add_shader(ssao_compose.frag         ssao/ssao_compose.frag "")
add_shader(ssao_compose_rq.frag      ssao/ssao_compose.frag -DRAY_QUERY)

add_shader(hiZRaw.comp    hiZPot.comp    "")
add_shader(hiZPot.comp    hiZPot.comp    -DRESOLVE)
add_shader(hiZMip.comp    hiZMip.comp    "")

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
