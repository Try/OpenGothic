#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/probe_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D probesLighting;
layout(binding = 2, std430) /*readonly*/ buffer Pbo { ProbesHeader probeHeader; Probe probe[]; };

layout(location = 0) in  vec3  center;
layout(location = 1) in  float radius;
layout(location = 2) in  flat uint instanceIndex;
layout(location = 3) in  flat uint isHashed;

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
  return vec3(light);
  }

void main(void) {
  vec3 pos1 = unprojectDepth(gl_FragCoord.z);
  vec3 pos0 = unprojectDepth(0);
  vec3 view = normalize(pos1-pos0);

  vec4 sp = scene.viewProject * vec4(center, 1.0);
  sp.xyz /= sp.w;

  bool watch = false; //abs(sp.x)<0.05 && abs(sp.y)<0.05;
  if(watch)
    probeHeader.watch = instanceIndex;

  float t = rayIntersect(pos0-center, view, radius);
  if(t<0 && !watch)
    discard;

  Probe p = probe[instanceIndex];

  vec3 norm = normalize((pos0+view*t) - p.pos);
  // vec3 clr  = lodColor(p, norm);
  vec3 clr  = probeReadAmbient(probesLighting, instanceIndex, norm, p.normal) * scene.exposure;
  if((p.bits & BAD_BIT)!=0)
    clr = vec3(1,0,0);
  if((p.bits & NEW_BIT)!=0)
    clr = vec3(0,1,0);
  if(isHashed==0)
    clr = vec3(0,0,1);

  outColor = vec4(clr,1.0);
  // outColor = vec4(1,0,0,1.0);
  }
