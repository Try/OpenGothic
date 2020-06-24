cmake_minimum_required(VERSION 3.12)

project(GothicShaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/*.glsl")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator)
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
    BYPRODUCTS ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

# shaders
# Ubershader flags:
#   OBJ        - enable object matrix
#   LIGHT_EXT  - shader for extra lights
#   SKINING    - animation skeleton
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation
#   G1         - hint for gothic1 shader
add_shader(obj.vert             main.vert -DOBJ)
add_shader(obj.frag             main.frag -DOBJ)
add_shader(obj_at.vert          main.vert -DOBJ -DATEST)
add_shader(obj_at.frag          main.frag -DOBJ -DATEST)
add_shader(ani.vert             main.vert -DOBJ -DSKINING)
add_shader(ani.frag             main.frag -DOBJ -DSKINING)
add_shader(ani_at.vert          main.vert -DOBJ -DSKINING -DATEST)
add_shader(ani_at.frag          main.frag -DOBJ -DSKINING -DATEST)
add_shader(pfx.vert             main.vert -DPFX)
add_shader(pfx.frag             main.frag -DPFX)

add_shader(obj_shadow.vert      main.vert -DSHADOW_MAP -DOBJ)
add_shader(obj_shadow.frag      main.frag -DSHADOW_MAP -DOBJ)
add_shader(obj_at_shadow.vert   main.vert -DSHADOW_MAP -DOBJ -DATEST)
add_shader(obj_at_shadow.frag   main.frag -DSHADOW_MAP -DOBJ -DATEST)
add_shader(ani_shadow.vert      main.vert -DSHADOW_MAP -DOBJ -DSKINING)
add_shader(ani_shadow.frag      main.frag -DSHADOW_MAP -DOBJ -DSKINING)
add_shader(ani_at_shadow.vert   main.vert -DSHADOW_MAP -DOBJ -DSKINING -DATEST)
add_shader(ani_at_shadow.frag   main.frag -DSHADOW_MAP -DOBJ -DSKINING -DATEST)
add_shader(pfx_shadow.vert      main.vert -DSHADOW_MAP)
add_shader(pfx_shadow.frag      main.frag -DSHADOW_MAP)

add_shader(obj_light.vert       main.vert -DLIGHT_EXT -DOBJ)
add_shader(obj_light.frag       main.frag -DLIGHT_EXT -DOBJ)
add_shader(obj_at_light.vert    main.vert -DLIGHT_EXT -DOBJ -DATEST)
add_shader(obj_at_light.frag    main.frag -DLIGHT_EXT -DOBJ -DATEST)
add_shader(ani_light.vert       main.vert -DLIGHT_EXT -DOBJ -DSKINING)
add_shader(ani_light.frag       main.frag -DLIGHT_EXT -DOBJ -DSKINING)
add_shader(ani_at_light.vert    main.vert -DLIGHT_EXT -DOBJ -DSKINING -DATEST)
add_shader(ani_at_light.frag    main.frag -DLIGHT_EXT -DOBJ -DSKINING -DATEST)
add_shader(pfx_light.vert       main.vert -DOBJ -DLIGHT_EXT)
add_shader(pfx_light.frag       main.frag -DOBJ -DLIGHT_EXT)

add_shader(sky.vert              sky.vert "")
add_shader(sky.frag              sky.frag "")
add_shader(sky_g1.vert           sky.vert -DG1)
add_shader(sky_g1.frag           sky.frag -DG1)
add_shader(shadow_compose.vert   shadow_compose.vert "")
add_shader(shadow_compose.frag   shadow_compose.frag "")

add_custom_command(
  OUTPUT     ${HEADER} ${CPP}
  BYPRODUCTS ${HEADER} ${CPP}
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
