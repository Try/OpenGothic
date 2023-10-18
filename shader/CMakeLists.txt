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

  list(LENGTH ARGV ARGC)
  if(ARGC GREATER 2)
    list(SUBLIST ARGV 2 -1 VARS)
  endif()

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
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} -I"${CMAKE_SOURCE_DIR}/shader" "${SOURCE_FILE}" -o ${OUTPUT_FILE}
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
  add_shader(${OUTPUT}.frag                  materials/main.frag ${VARS} -DFORWARD -DMAT_LINEAR_CLR)

  add_shader(${OUTPUT}_at.vert               materials/main.vert ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.32.mesh            materials/main.mesh ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.64.mesh            materials/main.mesh ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.32.task            materials/main.task ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.64.task            materials/main.task ${VARS} -DFORWARD -DATEST)
  add_shader(${OUTPUT}_at.frag               materials/main.frag ${VARS} -DFORWARD -DATEST -DMAT_LINEAR_CLR)

  add_shader(${OUTPUT}_emi.vert              materials/main.vert ${VARS})
  add_shader(${OUTPUT}_emi.32.mesh           materials/main.mesh ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_emi.64.mesh           materials/main.mesh ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_emi.32.task           materials/main.task ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_emi.64.task           materials/main.task ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_emi.frag              materials/main.frag ${VARS} -DEMISSIVE -DMAT_LINEAR_CLR)

  add_shader(${OUTPUT}_mul.vert              materials/main.vert ${VARS})
  add_shader(${OUTPUT}_mul.32.mesh           materials/main.mesh ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_mul.64.mesh           materials/main.mesh ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_mul.32.task           materials/main.task ${VARS} -DWG_SIZE=32)
  add_shader(${OUTPUT}_mul.64.task           materials/main.task ${VARS} -DWG_SIZE=64)
  add_shader(${OUTPUT}_mul.frag              materials/main.frag ${VARS})

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
  add_shader(${OUTPUT}_water.frag            materials/main.frag ${VARS} -DFORWARD -DWATER -DMAT_LINEAR_CLR)

  add_shader(${OUTPUT}_ghost.vert            materials/main.vert ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.32.mesh         materials/main.mesh ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.64.mesh         materials/main.mesh ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.32.task         materials/main.task ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.64.task         materials/main.task ${VARS} -DFORWARD -DGHOST)
  add_shader(${OUTPUT}_ghost.frag            materials/main.frag ${VARS} -DFORWARD -DGHOST -DMAT_LINEAR_CLR)

  set(OPENGOTHIC_SHADERS      ${OPENGOTHIC_SHADERS}      PARENT_SCOPE)
  set(OPENGOTHIC_SHADERS_SPRV ${OPENGOTHIC_SHADERS_SPRV} PARENT_SCOPE)
endfunction(add_uber_shader)

# shaders
add_uber_shader(lnd "-DMESH_TYPE=0")
add_uber_shader(obj "-DMESH_TYPE=1")
add_uber_shader(ani "-DMESH_TYPE=2")
add_uber_shader(mph "-DMESH_TYPE=3")
add_uber_shader(pfx "-DMESH_TYPE=4")

add_shader(item.vert                 inventory/item.vert)
add_shader(item.frag                 inventory/item.frag)

add_shader(lnd_hiz.32.mesh           materials/main.mesh -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.64.mesh           materials/main.mesh -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.32.task           materials/main.task -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.64.task           materials/main.task -DMESH_TYPE=0 -DHIZ)
add_shader(lnd_hiz.frag              materials/main.frag -DMESH_TYPE=0 -DHIZ)

add_shader(stash.vert                copy.vert)
add_shader(stash.frag                stash.frag)

add_shader(shadow_resolve.vert       copy.vert)
add_shader(shadow_resolve.frag       lighting/shadow_resolve.frag)
add_shader(shadow_resolve_sh.frag    lighting/shadow_resolve.frag -DSHADOW_MAP)
add_shader(shadow_resolve_rq.frag    lighting/shadow_resolve.frag -DSHADOW_MAP -DRAY_QUERY -DRAY_QUERY_AT)

add_shader(light.vert                lighting/light.vert)
add_shader(light.frag                lighting/light.frag)
add_shader(light_rq.frag             lighting/light.frag -DRAY_QUERY)
add_shader(light_rq_at.frag          lighting/light.frag -DRAY_QUERY -DRAY_QUERY_AT)

add_shader(irradiance.comp           lighting/irradiance.comp)

add_shader(tonemapping.vert          copy.vert -DHAS_UV)
add_shader(tonemapping.frag          lighting/tonemapping.frag)

add_shader(water_reflection.vert     copy.vert)
add_shader(water_reflection.mesh     water/water_reflection.mesh)
add_shader(water_reflection.frag     water/water_reflection.frag)
add_shader(water_reflection_ssr.frag water/water_reflection.frag -DSSR)

add_shader(underwater.vert           copy.vert "")
add_shader(underwater_t.frag         water/underwater.frag "")
add_shader(underwater_s.frag         water/underwater.frag -DSCATTERING)

# Based on Scalable and Production Ready Sky and Atmosphere
# https://sebh.github.io/publications/egsr2020.pdf
# https://www.shadertoy.com/view/slSXRW
add_shader(sky.vert                  sky/sky.vert)

add_shader(sky_transmittance.frag    sky/sky_transmittance.frag)
add_shader(sky_multi_scattering.frag sky/sky_multi_scattering.frag)
add_shader(sky_view_lut.frag         sky/sky_view_lut.frag)
add_shader(sky_view_clouds_lut.frag  sky/sky_view_clouds_lut.frag)

add_shader(shadow_downsample.comp    sky/shadow_downsample.comp)
add_shader(clouds_lut.comp           sky/clouds_lut.comp)

add_shader(fog_view_lut_lq.comp      sky/fog_view_lut.comp -DVOLUMETRIC_LQ -DCOMPUTE)
add_shader(fog_view_lut_hq.comp      sky/fog_view_lut.comp -DVOLUMETRIC_HQ -DCOMPUTE)

add_shader(fog3d_lq.frag             sky/fog.frag -S frag -DVOLUMETRIC_LQ)
add_shader(fog3d_hq.frag             sky/fog.frag -S frag -DVOLUMETRIC_HQ)
add_shader(fog3d.comp                sky/fog.frag -S comp -DVOLUMETRIC_HQ -DCOMPUTE)

add_shader(fog.frag                  sky/fog.frag)
add_shader(sky.frag                  sky/sky.frag)

add_shader(sun.vert                  sky/sun.vert)
add_shader(sun.frag                  sky/sun.frag)

add_shader(copy.vert                 copy.vert -DHAS_UV)
add_shader(copy.frag                 copy.frag)
add_shader(copy.comp                 copy.comp)
add_shader(copy_img.comp             copy_img.comp)
add_shader(path.comp                 path.comp)

add_shader(ssao.comp                 ssao/ssao.comp)

add_shader(ssao_compose.vert         copy.vert -DHAS_UV)
add_shader(ssao_compose.frag         ssao/ssao_compose.frag)
add_shader(ssao_compose_ssao.frag    ssao/ssao_compose.frag -DSSAO)

add_shader(hiz_mip.comp              hiz/hiz_mip.comp)
add_shader(hiz_pot.comp              hiz/hiz_pot.comp)

add_shader(hiz_reproject.mesh        hiz/hiz_reproject.mesh)
add_shader(hiz_reproject.frag        hiz/hiz_reproject.frag)

add_shader(probe_dbg.vert            lighting/rt/probe_dbg.vert)
add_shader(probe_dbg.frag            lighting/rt/probe_dbg.frag)

add_shader(probe_init.comp           lighting/rt/probe_aux.comp        -DINIT)
add_shader(probe_clear.comp          lighting/rt/probe_aux.comp        -DCLEAR)
add_shader(probe_clear_hash.comp     lighting/rt/probe_aux.comp        -DCLEAR_HASH)
add_shader(probe_make_hash.comp      lighting/rt/probe_aux.comp        -DMAKE_HASH)
add_shader(probe_prune.comp          lighting/rt/probe_aux.comp        -DPRUNE)
add_shader(probe_vote.comp           lighting/rt/probe_allocation.comp -DVOTE)
add_shader(probe_allocation.comp     lighting/rt/probe_allocation.comp -DALLOC)
add_shader(probe_trace.comp          lighting/rt/probe_trace.comp      -DRAY_QUERY -DRAY_QUERY_AT)
add_shader(probe_lighting.comp       lighting/rt/probe_lighting.comp)

add_shader(probe_ambient.vert        copy.vert)
add_shader(probe_ambient.frag        lighting/rt/probe_ambient.frag)

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
