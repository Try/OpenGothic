#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"
#include "lighting/tonemapping.glsl"

layout(push_constant, std140) uniform PushConstant {
  float exposureInv;
  } push;

layout(binding  = 0) uniform sampler2D textureD;

layout(location = 0) out vec4 outColor;

void main() {
  float exposureInv = push.exposureInv;
  vec3  color       = texelFetch(textureD, ivec2(gl_FragCoord.xy), 0).rgb;

  // night shift
  // color += purkinjeShift(color/exposureInv)*exposureInv;

  // Tonemapping
  color = acesTonemap(color);
  color = srgbEncode(color);

  outColor = vec4(color, 1.0);
  }
