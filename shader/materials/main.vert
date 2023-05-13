#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "materials_common.glsl"
#include "vertex_process.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

#if defined(MAT_VARYINGS)
layout(location = 0) out Varyings shOut;
#endif

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) out flat uint debugId;
#endif

void main() {
#if defined(LVL_OBJECT)
  uint objId = gl_InstanceIndex;
#elif (MESH_TYPE==T_PFX)
  uint objId = gl_InstanceIndex;
#else
  uint objId = 0;
#endif

  Varyings var;
  gl_Position = processVertex(var, objId, gl_VertexIndex);
#if defined(MAT_VARYINGS)
  shOut = var;
#endif

#if DEBUG_DRAW
  debugId = gl_VertexIndex/64;
#endif
  }
