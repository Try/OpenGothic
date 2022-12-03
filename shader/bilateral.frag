#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

const float KERNEL_RADIUS = 3;

layout(binding  = 0) uniform sampler2D src;
layout(binding  = 1) uniform sampler2D depth;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform PushConstant {
  vec3  clipInfo;
  float sharpness;
  vec2  invResolutionDirection;
  } ubo;

float texLinearDepth(vec2 uv) {
  float d = texture(depth, uv).x;
  return linearDepth(d, ubo.clipInfo);
  }

vec4 blurFunction(vec2 uv, float r, vec4 centerC, float centerD, inout float wTotal) {
  vec4  c = texture(src, uv);
  float d = texLinearDepth(uv);

  const float blurSigma   = float(KERNEL_RADIUS) * 0.5;
  const float blurFalloff = 1.0/(2.0*blurSigma*blurSigma);

  float ddiff = (d - centerD) * ubo.sharpness;
  float w     = exp2(-r*r*blurFalloff - ddiff*ddiff);
  wTotal += w;

  return c*w;
  }

void main() {
  vec2  invResolutionDirection = ubo.invResolutionDirection;

  vec4  centerC = texture(src, uv);
  float centerD = texLinearDepth(uv);

  vec4  cTotal  = centerC;
  float wTotal  = 1.0;

  for(float r=1; r<=KERNEL_RADIUS; ++r) {
    vec2 at = uv + invResolutionDirection * r;
    cTotal += blurFunction(at, r, centerC, centerD, wTotal);
    }

  for(float r=1; r<=KERNEL_RADIUS; ++r) {
    vec2 at = uv - invResolutionDirection * r;
    cTotal += blurFunction(at, r, centerC, centerD, wTotal);
    }

  outColor = cTotal/wTotal;
  }
