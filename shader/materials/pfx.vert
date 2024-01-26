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
  const Vertex vert = pullVertex(gl_InstanceIndex);

  Varyings var;
  gl_Position = processVertex(var, vert, 0, gl_InstanceIndex, gl_VertexIndex);
#if defined(MAT_VARYINGS)
  shOut = var;
#endif
  }
