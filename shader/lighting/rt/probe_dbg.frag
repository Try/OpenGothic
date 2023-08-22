#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

layout(early_fragment_tests) in;

layout(location = 0) in  vec3 color;
layout(location = 1) in  vec3 center;

layout(location = 0) out vec4 outColor;

void main(void) {
  outColor = vec4(color,1.0);
  // outColor = vec4(1,0,0,1.0);
  }
