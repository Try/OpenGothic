#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D textureD;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormmal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
  vec4 color = texture(textureD,inUV)*vec4(inColor);
  if(color.a<0.45)
    discard;
  outColor = color;
  }
