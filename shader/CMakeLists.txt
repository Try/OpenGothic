cmake_minimum_required(VERSION 3.12)

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator required")
endif()

function(compile_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/opengothic/shader/${OUTPUT}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/shader/${SOURCE}")
  list(SUBLIST ARGV 2 -1 VARS)

  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/opengothic/shader/"
    COMMAND ${GLSLANGVALIDATOR} -V ${VARS} ${SOURCE_FILE} -o ${OUTPUT_FILE}
    DEPENDS ${SOURCE_FILE})
  set(OPENGOTHIC_SHADERS ${OPENGOTHIC_SHADERS} ${OUTPUT_FILE} PARENT_SCOPE)
endfunction(compile_shader)

# shaders
# Ubershader flags:
#   OBJ        - enable object matrix
#   SKINING    - animation skeleton
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation
compile_shader(land.vert            main.vert -DATEST)
compile_shader(land.frag            main.frag -DATEST)
compile_shader(object.vert          main.vert -DATEST -DOBJ)
compile_shader(object.frag          main.frag -DATEST -DOBJ)
compile_shader(anim.vert            main.vert -DATEST -DOBJ -DSKINING)
compile_shader(anim.frag            main.frag -DATEST -DOBJ -DSKINING)
compile_shader(pfx.vert             main.vert -DPFX)
compile_shader(pfx.frag             main.frag -DPFX)
compile_shader(land_shadow.vert     main.vert -DATEST -DSHADOW_MAP)
compile_shader(land_shadow.frag     main.frag -DATEST -DSHADOW_MAP)
compile_shader(object_shadow.vert   main.vert -DATEST -DOBJ -DSHADOW_MAP)
compile_shader(object_shadow.frag   main.frag -DATEST -DOBJ -DSHADOW_MAP)
compile_shader(anim_shadow.vert     main.vert -DOBJ -DSKINING -DSHADOW_MAP)
compile_shader(anim_shadow.frag     main.frag -DOBJ -DSKINING -DSHADOW_MAP)
compile_shader(pfx_shadow.vert      main.vert -DSHADOW_MAP)
compile_shader(pfx_shadow.frag      main.frag -DSHADOW_MAP)
compile_shader(sky.vert             sky.vert "")
compile_shader(sky.frag             sky.frag "")
compile_shader(sky_g1.vert          sky.vert -DG1)
compile_shader(sky_g1.frag          sky.frag -DG1)
compile_shader(shadow_compose.vert  shadow_compose.vert "")
compile_shader(shadow_compose.frag  shadow_compose.frag "")

# executable
add_executable(${PROJECT_NAME}
    ${OPENGOTHIC_SOURCES}
    ${OPENGOTHIC_SHADERS}
    icon.rc)
