#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/probe_common.glsl"
#include "scene.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2, std430) readonly buffer Pbo  { ProbesHeader probeHeader; Probe probe[]; };
layout(binding = 3, std430) readonly buffer Hbo0 { Hash hashTable[]; };

layout(location = 0) out vec3      center;
layout(location = 1) out flat uint instanceIndex;
layout(location = 2) out flat uint isHashed;


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
  const uint probeId = gl_InstanceIndex;
  if(probeId>=probeHeader.count) {
    gl_Position = vec4(0);
    return;
    }

  Probe p = probe[probeId];
  if((p.bits & UNUSED_BIT)!=0){
    gl_Position = vec4(0);
    return;
    }

  const vec3 vert = v[index[gl_VertexIndex]];

  gl_Position   = scene.viewProject * vec4(p.pos + vert * dbgViewRadius, 1.0);
  center        = p.pos;
  instanceIndex = probeId;

  const vec3 gridPos = probe[probeId].pos/probeGridStep;
  const uint h       = probeGridPosHash(ivec3(gridPos)) % hashTable.length();
  const uint cursor  = hashTable[h].value;
  isHashed = (cursor==probeId) ? 1 : 0;
  }
