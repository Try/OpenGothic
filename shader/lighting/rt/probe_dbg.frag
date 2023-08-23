#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/probe_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, std430) readonly buffer Pbo { ProbesHeader probeHeader; Probe probe[]; };

layout(location = 0) in  vec3 center;
layout(location = 1) in  flat uint instanceIndex;

layout(location = 0) out vec4 outColor;

vec3 unprojectDepth(float z) {
  const vec2 fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4 pos       = vec4(fragCoord.xy, z, 1.0);///gl_FragCoord.w);
  const vec4 ret       = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

vec3 lodColor(const in Probe p, vec3 norm) {
  float lamb  = max(dot(scene.sunDir, norm), 0);
  float light = lamb*0.7+0.3;
  return p.color[0][0]*light;
  }

vec3 irradiance(const in Probe p, vec3 n) {
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

void main(void) {
  vec3 pos1 = unprojectDepth(gl_FragCoord.z);
  vec3 pos0 = unprojectDepth(0);
  vec3 view = normalize(pos1-pos0);

  float t = rayIntersect(pos0-center, view, dbgViewRadius);
  if(t<0) {
    outColor = vec4(1,0,0,0);
    discard;
    }

  Probe p = probe[instanceIndex];

  vec3 norm = normalize((pos0+view*t) - p.pos);
  // vec3 clr  = lodColor(p, norm);
  vec3 clr  = irradiance(p, norm);

  outColor = vec4(clr,1.0);
  // outColor = vec4(1,0,0,1.0);
  }
