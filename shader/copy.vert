#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in  vec2 inPos;

#if defined(HAS_UV)
layout(location = 0) out vec2 UV;
#endif

void main() {
#if defined(HAS_UV)
  UV          = inPos*vec2(0.5)+vec2(0.5);
#endif
  gl_Position = vec4(inPos.xy, 1.0, 1.0);
  }
