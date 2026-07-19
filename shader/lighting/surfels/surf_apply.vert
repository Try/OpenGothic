#version 450

#include "lighting/surfels/surf_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec3  surfPos;
layout(location = 1) out float surfRadius;
layout(location = 2) out vec3  surfNorm;
layout(location = 3) out vec4  surfIrr;

layout(std140, push_constant) uniform Push {
  vec3 originLwc;
  uint tileSize;
  uint pass;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, rgba16f) uniform image2D irradiance;
layout(binding = 2) uniform utexture2D gbufNormal;
layout(binding = 3) uniform texture2D  depth;
layout(binding = 4, std430) readonly buffer SB0 { SurfHeader header; Surfel surfels[]; };

const ivec2 vert[] = ivec2[6](
  ivec2(-1, -1),
  ivec2(+1, -1),
  ivec2(-1, +1),

  ivec2(-1, +1),
  ivec2(+1, -1),
  ivec2(+1, +1)
  );

void main() {
  const uint id = gl_InstanceIndex;
  if(id >= header.count) {
    gl_Position = vec4(uintBitsToFloat(0x7fc00000));
    return;
    }

  const Surfel s = surfels[id];
  surfPos    = s.pos;
  surfRadius = s.radius;
  surfNorm   = decodeNormal(s.norm);
  surfIrr    = vec4(s.irradiance, s.radiusMean);

  vec2 pos = (s.fragCoord + DefaultCoverage*vert[gl_VertexIndex])*scene.screenResInv;
  gl_Position = vec4(pos*2.0-1.0, 0, 1);
  }
