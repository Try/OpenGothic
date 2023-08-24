#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/probe_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding  = 0) uniform sampler2D gbufDiffuse;
layout(binding  = 1) uniform sampler2D gbufNormal;
layout(binding  = 2) uniform sampler2D depth;
layout(binding  = 3, std430) readonly buffer Pbo { ProbesHeader probeHeader; Probe probe[]; };
layout(binding  = 4, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 ambient(const in Probe p, vec3 n) {
  ivec3 d;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = n*n;

  vec3 ret = vec3(0);
  ret += p.color[0][d.x].rgb * n.x;
  ret += p.color[1][d.y].rgb * n.y;
  ret += p.color[2][d.z].rgb * n.z;
  return ret * scene.exposure;
  }

vec3 ambient() {
  vec3 n = texelFetch(gbufNormal, ivec2(gl_FragCoord.xy), 0).rgb;
  n = normalize(n*2.0 - vec3(1.0));

  const float z   = texelFetch(depth,ivec2(gl_FragCoord.xy),0).x;
  //const vec3  pos = unprojectDepth(z);
  return vec3(0);
  }

void main() {
  vec3  diff   = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;

  vec3  linear = textureLinear(diff);
  vec3  lcolor = ambient();

  vec3  color  = linear*lcolor;
  //color *= push.exposure;

  outColor = vec4(color, 1);
  }
