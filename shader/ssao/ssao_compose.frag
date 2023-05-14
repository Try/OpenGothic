#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "lighting/tonemapping.glsl"
#include "common.glsl"

const int   KERNEL_RADIUS = 1;
const float blurSharpness = 0.8;

layout(push_constant, std140) uniform PushConstant {
  vec3  ambient;
  float exposureInv;
  vec3  ldir;
  vec3  clipInfo;
  } push;

layout(binding  = 0) uniform sampler2D gbufDiffuse;
layout(binding  = 1) uniform sampler2D gbufNormal;
layout(binding  = 2) uniform sampler2D depth;
layout(binding  = 3) uniform sampler2D ssao;
layout(binding  = 4) uniform sampler2D irradiance;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

float texLinearDepth(vec2 uv) {
  float d = textureLod(depth, uv, 0).x;
  return linearDepth(d, push.clipInfo);
  }

float blurFunction(vec2 uv, float r, float centerD, inout float wTotal) {
  float c = textureLod(ssao, uv, 0).r;
  float d = texLinearDepth(uv);

  const float blurSigma   = float(KERNEL_RADIUS) * 0.5;
  const float blurFalloff = 1.0/(2.0*blurSigma*blurSigma);

  float ddiff = (d - centerD) * blurSharpness;
  float w     = exp2(-r*r*blurFalloff - ddiff*ddiff);
  wTotal += w;

  return c*w;
  }

float smoothSsao() {
  float centerC = textureLod(ssao, uv, 0).r;
  float centerD = texLinearDepth(uv);

  float cTotal  = centerC;
  float wTotal  = 1.0;

  vec2 invRes = vec2(1.0)/vec2(textureSize(ssao,0));
  for(float i=-KERNEL_RADIUS; i<=KERNEL_RADIUS; ++i)
    for(float r=-KERNEL_RADIUS; r<=KERNEL_RADIUS; ++r) {
      if((i==0 && r==0)) // || (abs(i)==KERNEL_RADIUS && abs(r)==KERNEL_RADIUS))
        continue;
      vec2 at = uv + invRes * vec2(i,r);
      cTotal += blurFunction(at, r, centerD, wTotal);
      }

  // return 0;
  return clamp(cTotal/wTotal, 0, 1);
  }

vec3 ambient() {
#if 0
  return push.ambient;
#else
  vec3 n = texelFetch(gbufNormal, ivec2(gl_FragCoord.xy), 0).rgb;
  n = normalize(n*2.0 - vec3(1.0));

  ivec3 d;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = abs(n);

  vec3 ret = vec3(0);
  ret += texelFetch(irradiance, ivec2(0,d.x), 0).rgb * n.x;
  ret += texelFetch(irradiance, ivec2(1,d.y), 0).rgb * n.y;
  ret += texelFetch(irradiance, ivec2(2,d.z), 0).rgb * n.z;
  return ret + push.ambient;
#endif
  }

void main() {
  vec3  diff   = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;
  float occ    = smoothSsao();

  vec3  linear = textureLinear(diff) * PhotoLumInv;
  vec3  lcolor = ambient();

  vec3  color  = linear*lcolor;
  color *= push.exposureInv;

  outColor = vec4(color*(1-occ), 1);
  }
