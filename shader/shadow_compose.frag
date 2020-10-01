#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D texture0;
layout(binding = 1) uniform sampler2D texture1;

layout(location = 0) in  vec2 UV;
layout(location = 0) out vec4 outColor;

void main() {
  vec4 a = texture(texture0,UV);
  vec4 b = texture(texture1,UV);

  outColor = vec4(a.r,b.g,0.0,0.0);
  }
