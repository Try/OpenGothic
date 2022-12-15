#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D textureD;

layout(location = 0) in  vec2 UV;
layout(location = 0) out vec4 outColor;

void main() {
  vec4 t = texture(textureD,UV);
  if(t.a<0.5)
    discard;
  outColor = t;
  }
