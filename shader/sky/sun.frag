#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

layout(binding  = 0) uniform sampler2D sunSprite;

layout(push_constant, std430) uniform UboPush {
  vec2  sunPos;
  vec2  sunSz;
  float GSunIntensity;
  uint  isSun;
  } push;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
  const vec4 clr = texture(sunSprite, uv);
  const vec3 lum = clr.rgb * push.GSunIntensity;

  outColor = vec4(lum, clr.a);
  }
