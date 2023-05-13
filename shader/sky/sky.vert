#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec2 outPos;

void main() {
  outPos      = inPos;
  gl_Position = vec4(inPos.xy, 1.0, 1.0);
  }
