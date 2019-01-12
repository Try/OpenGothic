#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormmal;
layout(location = 2) in vec4 inColor;

void main() {
  outColor = vec4(inColor);
  }
