#version 450

#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "lighting/rt/probe_common.glsl"
#include "common.glsl"
#include "scene.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2) uniform texture2D gbufferDiff;
layout(binding = 3) uniform texture2D gbufferRayT;
layout(binding = 4, std430) readonly buffer Pbo  { ProbesHeader probeHeader; Probe probe[]; };

layout(location = 0) out vec3      center;
layout(location = 1) out float     radius;
layout(location = 2) out flat uint instanceIndex;

const vec3 v[8] = {
  {-1,-1,-1},
  { 1,-1,-1},
  { 1, 1,-1},
  {-1, 1,-1},

  {-1,-1, 1},
  { 1,-1, 1},
  { 1, 1, 1},
  {-1, 1, 1},
  };

const uint index[36] = {
  0, 1, 3, 3, 1, 2,
  1, 5, 2, 2, 5, 6,
  5, 4, 6, 6, 4, 7,
  4, 0, 7, 7, 0, 3,
  3, 2, 7, 7, 2, 6,
  4, 5, 0, 0, 5, 1
  };

void main() {
  const uint samplesTotal = 256;
  const uint probeId      = gl_InstanceIndex / samplesTotal;
  const uint sampleId     = gl_InstanceIndex % samplesTotal;

  if(probeId>=probeHeader.count || probeId==0) {
    gl_Position = vec4(0);
    return;
    }

  Probe p = probe[probeId];
  if((p.bits & UNUSED_BIT)!=0 || (p.bits & TRACED_BIT)==0){
    gl_Position = vec4(0);
    return;
    }

  const vec3 rayDirection = sampleSphere(sampleId, samplesTotal, 0);

  const ivec2 uv   = gbufferCoord(probeId, sampleId);
  const float rt   = texelFetch(gbufferRayT, uv, 0).r;
  const float rayT = rt * probeRayDistance;
  const vec3  pos  = p.pos + rayT*rayDirection;

  if(rt>=1.0) {
    gl_Position = vec4(0);
    return;
    }

  const vec3 vert = v[index[gl_VertexIndex]];
  const vec4 pp   = scene.viewProject * vec4(pos, 1.0);

  radius        = dbgViewRadius;// * pp.w;
  gl_Position   = scene.viewProject * vec4(pos + vert * radius, 1.0);
  center        = pos;
  instanceIndex = probeId;
  }
