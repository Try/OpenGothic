#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/probe_common.glsl"
#include "lighting/rt/rt_common.glsl"
#include "scene.glsl"

const vec3 debugColors[] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2, rg32ui) uniform readonly uimage2D vbufferRayHit;
layout(binding = 3, std430) buffer Sbo { GiSceneHeader sceneHeader; uint hashTable[]; };

layout(location = 0) out vec3 outColor;

uvec2 unpackUVec2(uint v) {
  return uvec2(v & 0xFFFF, (v&0xFFFF0000)>>16);
  }

void main() {
  if(gl_InstanceIndex >= sceneHeader.count) {
    gl_Position = vec4(uintBitsToFloat(0x7fc00000));
    return;
    }

  uint  iid   = hashTable[gl_InstanceIndex];
  uvec2 index = unpackUVec2(iid);
  if(iid==0) {
    gl_Position = vec4(uintBitsToFloat(0x7fc00000));
    return;
    }

  const uvec2 vis         = imageLoad(vbufferRayHit, ivec2(index)).xy;
  const uint  instanceId  = vis.x;
  const uint  primitiveId = vis.y;
  const uint  vindex      = pullTrinagleIds(instanceId, primitiveId)[gl_VertexIndex];

  const vec3 pos = pullPosition(instanceId,vindex);

  outColor    = debugColors[iid%debugColors.length()];
  gl_Position = scene.viewProject * vec4(pos, 1);
  }
