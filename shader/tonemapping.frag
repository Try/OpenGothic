#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"
#include "lighting/tonemapping.glsl"

layout(push_constant, std140) uniform PushConstant {
  float exposureInv;
  } push;

layout(binding  = 0) uniform sampler2D textureD;

layout(location = 0) in  vec2 UV;
layout(location = 0) out vec4 outColor;

void main() {
  vec4 t     = textureLod(textureD,UV,0);
  vec3 color = t.rgb;

  // Color grading
  // color = pow(color, vec3(1.3));

  // Tonemapping
  color = jodieReinhardTonemap(color * push.exposureInv);
  // color = reinhardTonemap(color * exposure);
  color = srgbEncode(color);

  outColor = vec4(color, 1.0);
  }
