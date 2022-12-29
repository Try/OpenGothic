#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common.glsl"
#include "../tonemapping.glsl"

layout(push_constant, std140) uniform PushConstant {
  vec3  whitePoint;
  float exposure;
  };

layout(binding  = 0) uniform sampler2D textureD;

layout(location = 0) in  vec2 UV;
layout(location = 0) out vec4 outColor;

vec3 tonemapping2(vec3 color) {
  // Similar setup to the Bruneton demo
  return vec3(1.0) - exp(-color.rgb / (whitePoint * exposure));
  }

void main() {
  vec4 t     = textureLod(textureD,UV,0);
  vec3 color = t.rgb;

  // color = tonemapping2(color);
  color = jodieReinhardTonemap(color * exposure);
  // color = acesTonemap(color);

  color = srgbEncode(color);
  outColor = vec4(color, 1.0);
  }
