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
    DEPENDS    ${SOURCE_FILE}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

# shaders
# Ubershader flags:
#   OBJ        - enable object matrix
#   SKINING    - animation skeleton
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation
#   WATER      - water material
#   MORPH      - morphing animation
#   G1         - hint for gothic1 shader
add_shader(obj.vert             main.vert -DOBJ)
add_shader(obj.frag             main.frag -DOBJ)
add_shader(obj_at.vert          main.vert -DOBJ -DATEST)
add_shader(obj_at.frag          main.frag -DOBJ -DATEST)
add_shader(obj_emi.vert         main.vert -DOBJ -DEMMISSIVE)
add_shader(obj_emi.frag         main.frag -DOBJ -DEMMISSIVE)
add_shader(obj_gbuffer.vert     main.vert -DOBJ -DGBUFFER)
add_shader(obj_gbuffer.frag     main.frag -DOBJ -DGBUFFER)
add_shader(obj_at_gbuffer.vert  main.vert -DOBJ -DGBUFFER -DATEST)
add_shader(obj_at_gbuffer.frag  main.frag -DOBJ -DGBUFFER -DATEST)
add_shader(obj_shadow.vert      main.vert -DOBJ -DSHADOW_MAP)
add_shader(obj_shadow.frag      main.frag -DOBJ -DSHADOW_MAP)
add_shader(obj_shadow_at.vert   main.vert -DOBJ -DSHADOW_MAP -DATEST)
add_shader(obj_shadow_at.frag   main.frag -DOBJ -DSHADOW_MAP -DATEST)
add_shader(obj_water.vert       main.vert -DOBJ -DWATER)
add_shader(obj_water.frag       main.frag -DOBJ -DWATER)
add_shader(obj_ghost.vert       main.vert -DOBJ -DGHOST)
add_shader(obj_ghost.frag       main.frag -DOBJ -DGHOST)

add_shader(ani.vert             main.vert -DOBJ -DSKINING)
add_shader(ani.frag             main.frag -DOBJ -DSKINING)
add_shader(ani_at.vert          main.vert -DOBJ -DSKINING -DATEST)
add_shader(ani_at.frag          main.frag -DOBJ -DSKINING -DATEST)
add_shader(ani_emi.vert         main.vert -DOBJ -DSKINING -DEMMISSIVE)
add_shader(ani_emi.frag         main.frag -DOBJ -DSKINING -DEMMISSIVE)
add_shader(ani_gbuffer.vert     main.vert -DOBJ -DSKINING -DGBUFFER)
add_shader(ani_gbuffer.frag     main.frag -DOBJ -DSKINING -DGBUFFER)
add_shader(ani_at_gbuffer.vert  main.vert -DOBJ -DSKINING -DGBUFFER -DATEST)
add_shader(ani_at_gbuffer.frag  main.frag -DOBJ -DSKINING -DGBUFFER -DATEST)
add_shader(ani_shadow.vert      main.vert -DOBJ -DSKINING -DSHADOW_MAP)
add_shader(ani_shadow.frag      main.frag -DOBJ -DSKINING -DSHADOW_MAP)
add_shader(ani_shadow_at.vert   main.vert -DOBJ -DSKINING -DSHADOW_MAP -DATEST)
add_shader(ani_shadow_at.frag   main.frag -DOBJ -DSKINING -DSHADOW_MAP -DATEST)
add_shader(ani_water.vert       main.vert -DOBJ -DSKINING -DWATER)
add_shader(ani_water.frag       main.frag -DOBJ -DSKINING -DWATER)
add_shader(ani_ghost.vert       main.vert -DOBJ -DSKINING -DGHOST)
add_shader(ani_ghost.frag       main.frag -DOBJ -DSKINING -DGHOST)

add_shader(mph.vert             main.vert -DOBJ -DMORPH)
add_shader(mph.frag             main.frag -DOBJ -DMORPH)
add_shader(mph_at.vert          main.vert -DOBJ -DMORPH -DATEST)
add_shader(mph_at.frag          main.frag -DOBJ -DMORPH -DATEST)
add_shader(mph_emi.vert         main.vert -DOBJ -DMORPH -DEMMISSIVE)
add_shader(mph_emi.frag         main.frag -DOBJ -DMORPH -DEMMISSIVE)
add_shader(mph_gbuffer.vert     main.vert -DOBJ -DMORPH -DGBUFFER)
add_shader(mph_gbuffer.frag     main.frag -DOBJ -DMORPH -DGBUFFER)
add_shader(mph_at_gbuffer.vert  main.vert -DOBJ -DMORPH -DGBUFFER -DATEST)
add_shader(mph_at_gbuffer.frag  main.frag -DOBJ -DMORPH -DGBUFFER -DATEST)
add_shader(mph_shadow.vert      main.vert -DOBJ -DMORPH -DSHADOW_MAP)
add_shader(mph_shadow.frag      main.frag -DOBJ -DMORPH -DSHADOW_MAP)
add_shader(mph_shadow_at.vert   main.vert -DOBJ -DMORPH -DSHADOW_MAP -DATEST)
add_shader(mph_shadow_at.frag   main.frag -DOBJ -DMORPH -DSHADOW_MAP -DATEST)
add_shader(mph_water.vert       main.vert -DOBJ -DMORPH -DWATER)
add_shader(mph_water.frag       main.frag -DOBJ -DMORPH -DWATER)
add_shader(mph_ghost.vert       main.vert -DOBJ -DMORPH -DGHOST)
add_shader(mph_ghost.frag       main.frag -DOBJ -DMORPH -DGHOST)

add_shader(light.vert           light.vert "")
add_shader(light.frag           light.frag "")

add_shader(fog.vert             sky.vert -DFOG)
add_shader(fog.frag             sky.frag -DFOG)
add_shader(sky_g2.vert          sky.vert -DG2)
add_shader(sky_g2.frag          sky.frag -DG2)
add_shader(sky_g1.vert          sky.vert -DG1)
add_shader(sky_g1.frag          sky.frag -DG1)

add_shader(shadow_compose.vert  shadow_compose.vert "")
add_shader(shadow_compose.frag  shadow_compose.frag "")
add_shader(copy.vert            copy.vert "")
add_shader(copy.frag            copy.frag "")

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
