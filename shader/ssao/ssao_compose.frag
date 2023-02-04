#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "../lighting/tonemapping.glsl"
#include "../common.glsl"

const int   KERNEL_RADIUS = 1;
const float blurSharpness = 0.8;

layout(push_constant, std140) uniform PushConstant {
  vec3 ambient;
  vec3 ldir;
  vec3 clipInfo;
  } ubo;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D normals;
layout(binding  = 3) uniform sampler2D depth;
layout(binding  = 4) uniform sampler2D ssao;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

float texLinearDepth(vec2 uv) {
  float d = textureLod(depth, uv, 0).x;
  return linearDepth(d, ubo.clipInfo);
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

void main() {
  vec4  lbuf    = textureLod(lightingBuf,uv,0);
  vec3  clr     = textureLod(diffuse,    uv,0).rgb;
  float occ     = smoothSsao();

  vec3  linear  = textureLinear(clr.rgb);
  vec3  ambient = ubo.ambient;

  // outColor = vec4(1-occ);
  outColor = vec4(lbuf.rgb - clr*ambient*occ, lbuf.a);
  }
