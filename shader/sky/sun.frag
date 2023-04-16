#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "../common.glsl"

layout(binding = 0) uniform sampler2D sunSprite;
layout(binding = 1) uniform sampler2D tLUT;

layout(push_constant, std430) uniform UboPush {
  vec2  sunPos;
  vec2  sunSz;
  vec3  sunDir;
  float GSunIntensity;
  mat4  viewProjectInv;
  uint  isSun;
  } push;

layout(location = 0) in  vec2 uv;
layout(location = 1) in  vec2 inPos;

layout(location = 0) out vec4 outColor;

vec3 project(mat4 m, vec3 pos) {
  vec4 p = m*vec4(pos,1);
  return p.xyz/p.w;
  }

// 5.5.2. LUT parameterization
vec3 textureLUT(sampler2D tex, vec3 pos, vec3 sunDir) {
  float height   = length(pos);
  vec3  up       = pos / height;
  float cosAngle = dot(sunDir, up);

  vec2 uv;
  uv.x = 0.5 + 0.5*cosAngle;
  uv.y = (height - RPlanet)/(RAtmos - RPlanet);
  return texture(tex, uv).rgb;
  }

void main() {
  const vec3 pos  = vec3(0, RPlanet+1, 0);
  const vec3 pos0 = project(push.viewProjectInv, vec3(inPos,0.0));
  const vec3 pos1 = project(push.viewProjectInv, vec3(inPos,1.0));
  const vec3 view = normalize(pos1);

  const vec4 clr = texture(sunSprite, uv);
  vec3 lum = clr.rgb;
  if(push.isSun!=0) {
    lum *= textureLUT(tLUT, pos, push.sunDir).rgb;
    }

  lum *= push.GSunIntensity;

  if(rayIntersect(pos, view, RPlanet)>=0.0) {
    lum = vec3(0.0);
    }

  outColor = vec4(lum, clr.a);
  }
