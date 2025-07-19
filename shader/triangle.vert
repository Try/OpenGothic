#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

#if defined(HAS_UV)
layout(location = 0) out vec2 UV;
#endif

void main() {
  const vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
#if defined(HAS_UV)
  UV = outUV;
#endif
  gl_Position = vec4(outUV*2.0-1.0, 1, 1);
  }
