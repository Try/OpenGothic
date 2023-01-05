#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "lighting/tonemapping.glsl"
#include "common.glsl"

layout(push_constant, std140) uniform PushConstant {
  mat4 mvpInv;
  vec3 ambient;
  vec3 ldir;
  } ubo;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D ssao;
layout(binding  = 3) uniform sampler2D depth;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 inverse(vec3 pos) {
  vec4 ret = ubo.mvpInv*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

void main() {
  vec4  lbuf    = textureLod(lightingBuf,uv,0);
  vec3  clr     = textureLod(diffuse,    uv,0).rgb;
  vec3  linear  = acesTonemapInv(srgbDecode(clr.rgb));
  float occ     = textureLod(ssao,       uv,0).r;
  vec3  ambient = ubo.ambient;

  // outColor = vec4(1-occ);
  outColor = vec4(lbuf.rgb-clr*ambient*occ, lbuf.a);
  }
