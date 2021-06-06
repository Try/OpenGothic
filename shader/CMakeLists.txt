cmake_minimum_required(VERSION 3.12)

project(GothicShaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/shader/*.vert"
  "${CMAKE_SOURCE_DIR}/shader/*.frag"
  "${CMAKE_SOURCE_DIR}/shader/*.glsl")

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

# shaders
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
add_shader(clr_shadow.vert      main.vert -DVCOLOR -DSHADOW_MAP)
add_shader(clr_shadow.frag      main.frag -DVCOLOR -DSHADOW_MAP)
add_shader(clr_shadow_at.vert   main.vert -DVCOLOR -DSHADOW_MAP -DATEST)
add_shader(clr_shadow_at.frag   main.frag -DVCOLOR -DSHADOW_MAP -DATEST)
add_shader(clr.vert             main.vert -DVCOLOR -DLIGHT)
add_shader(clr.frag             main.frag -DVCOLOR -DLIGHT)
add_shader(clr_at.vert          main.vert -DVCOLOR -DLIGHT -DATEST)
add_shader(clr_at.frag          main.frag -DVCOLOR -DLIGHT -DATEST)
add_shader(clr_emi.vert         main.vert -DVCOLOR)
add_shader(clr_emi.frag         main.frag -DVCOLOR)
add_shader(clr_gbuffer.vert     main.vert -DVCOLOR -DLIGHT -DGBUFFER)
add_shader(clr_gbuffer.frag     main.frag -DVCOLOR -DLIGHT -DGBUFFER)
add_shader(clr_gbuffer_at.vert  main.vert -DVCOLOR -DLIGHT -DGBUFFER -DATEST)
add_shader(clr_gbuffer_at.frag  main.frag -DVCOLOR -DLIGHT -DGBUFFER -DATEST)
add_shader(clr_water.vert       main.vert -DVCOLOR -DLIGHT -DWATER)
add_shader(clr_water.frag       main.frag -DVCOLOR -DLIGHT -DWATER)
add_shader(clr_ghost.vert       main.vert -DVCOLOR -DLIGHT -DGHOST)
add_shader(clr_ghost.frag       main.frag -DVCOLOR -DLIGHT -DGHOST)

add_shader(obj_shadow.vert      main.vert -DOBJ -DSHADOW_MAP)
add_shader(obj_shadow.frag      main.frag -DOBJ -DSHADOW_MAP)
add_shader(obj_shadow_at.vert   main.vert -DOBJ -DSHADOW_MAP -DATEST)
add_shader(obj_shadow_at.frag   main.frag -DOBJ -DSHADOW_MAP -DATEST)
add_shader(obj.vert             main.vert -DOBJ -DLIGHT)
add_shader(obj.frag             main.frag -DOBJ -DLIGHT)
add_shader(obj_at.vert          main.vert -DOBJ -DLIGHT -DATEST)
add_shader(obj_at.frag          main.frag -DOBJ -DLIGHT -DATEST)
add_shader(obj_emi.vert         main.vert -DOBJ)
add_shader(obj_emi.frag         main.frag -DOBJ)
add_shader(obj_gbuffer.vert     main.vert -DOBJ -DLIGHT -DGBUFFER)
add_shader(obj_gbuffer.frag     main.frag -DOBJ -DLIGHT -DGBUFFER)
add_shader(obj_gbuffer_at.vert  main.vert -DOBJ -DLIGHT -DGBUFFER -DATEST)
add_shader(obj_gbuffer_at.frag  main.frag -DOBJ -DLIGHT -DGBUFFER -DATEST)
add_shader(obj_water.vert       main.vert -DOBJ -DLIGHT -DWATER)
add_shader(obj_water.frag       main.frag -DOBJ -DLIGHT -DWATER)
add_shader(obj_ghost.vert       main.vert -DOBJ -DLIGHT -DGHOST)
add_shader(obj_ghost.frag       main.frag -DOBJ -DLIGHT -DGHOST)

add_shader(ani_shadow.vert      main.vert -DSKINING -DSHADOW_MAP)
add_shader(ani_shadow.frag      main.frag -DSKINING -DSHADOW_MAP)
add_shader(ani_shadow_at.vert   main.vert -DSKINING -DSHADOW_MAP -DATEST)
add_shader(ani_shadow_at.frag   main.frag -DSKINING -DSHADOW_MAP -DATEST)
add_shader(ani.vert             main.vert -DSKINING -DLIGHT)
add_shader(ani.frag             main.frag -DSKINING -DLIGHT)
add_shader(ani_at.vert          main.vert -DSKINING -DLIGHT -DATEST)
add_shader(ani_at.frag          main.frag -DSKINING -DLIGHT -DATEST)
add_shader(ani_emi.vert         main.vert -DSKINING)
add_shader(ani_emi.frag         main.frag -DSKINING)
add_shader(ani_gbuffer.vert     main.vert -DSKINING -DLIGHT -DGBUFFER)
add_shader(ani_gbuffer.frag     main.frag -DSKINING -DLIGHT -DGBUFFER)
add_shader(ani_gbuffer_at.vert  main.vert -DSKINING -DLIGHT -DGBUFFER -DATEST)
add_shader(ani_gbuffer_at.frag  main.frag -DSKINING -DLIGHT -DGBUFFER -DATEST)
add_shader(ani_water.vert       main.vert -DSKINING -DLIGHT -DWATER)
add_shader(ani_water.frag       main.frag -DSKINING -DLIGHT -DWATER)
add_shader(ani_ghost.vert       main.vert -DSKINING -DLIGHT -DGHOST)
add_shader(ani_ghost.frag       main.frag -DSKINING -DLIGHT -DGHOST)

add_shader(mph_shadow.vert      main.vert -DMORPH -DSHADOW_MAP)
add_shader(mph_shadow.frag      main.frag -DMORPH -DSHADOW_MAP)
add_shader(mph_shadow_at.vert   main.vert -DMORPH -DSHADOW_MAP -DATEST)
add_shader(mph_shadow_at.frag   main.frag -DMORPH -DSHADOW_MAP -DATEST)
add_shader(mph.vert             main.vert -DMORPH -DLIGHT)
add_shader(mph.frag             main.frag -DMORPH -DLIGHT)
add_shader(mph_at.vert          main.vert -DMORPH -DLIGHT -DATEST)
add_shader(mph_at.frag          main.frag -DMORPH -DLIGHT -DATEST)
add_shader(mph_emi.vert         main.vert -DMORPH)
add_shader(mph_emi.frag         main.frag -DMORPH)
add_shader(mph_gbuffer.vert     main.vert -DMORPH -DLIGHT -DGBUFFER)
add_shader(mph_gbuffer.frag     main.frag -DMORPH -DLIGHT -DGBUFFER)
add_shader(mph_gbuffer_at.vert  main.vert -DMORPH -DLIGHT -DGBUFFER -DATEST)
add_shader(mph_gbuffer_at.frag  main.frag -DMORPH -DLIGHT -DGBUFFER -DATEST)
add_shader(mph_water.vert       main.vert -DMORPH -DLIGHT -DWATER)
add_shader(mph_water.frag       main.frag -DMORPH -DLIGHT -DWATER)
add_shader(mph_ghost.vert       main.vert -DMORPH -DLIGHT -DGHOST)
add_shader(mph_ghost.frag       main.frag -DMORPH -DLIGHT -DGHOST)

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
