#version 450

#include "lighting/surfels/surf_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, std430) readonly buffer SB0  { uvec4 count; Surfel surfels[]; };
layout(binding = 3) uniform sampler2D  depth;

layout(location = 0) out vec3      center;
layout(location = 1) out vec3      normal;
layout(location = 2) out float     radius;
layout(location = 3) out flat uint instanceIndex;

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

vec3 hasGridPos(vec3 wpos, float cellSize) {
  wpos = wpos / cellSize;
  return round(wpos);
  }

float pixelToWorld(float pixelRadius, float z) {
  z = linearDepth(z, scene.clipInfo);
  float clipRadius  = (2.0 * pixelRadius) * scene.screenResInv.y;
  float worldRadius = (clipRadius * z) / scene.project[1][1];
  return worldRadius;
  }

void main() {
  const uint surfelId = gl_InstanceIndex;
  if(surfelId>=count.x) {
    gl_Position = vec4(0);
    return;
    }

  Surfel p = surfels[surfelId];

  const vec3 vert = v[index[gl_VertexIndex]];
  const vec4 pp   = scene.viewProject * vec4(p.pos, 1.0);

  float pixelRadius = 5.0;
  float worldRadius = pixelToWorld(pixelRadius, pp.z/pp.w);

  //radius        = 5;
  radius        = worldRadius;
  gl_Position   = scene.viewProject * vec4(p.pos + vert * radius, 1.0);
  center        = p.pos;
  normal        = decodeNormal(p.norm);

  const float fov = 67.5f*M_PI/180.0;
  const float ld  = max(linearDepth(pp.z/pp.w, scene.clipInfo), 10);

  instanceIndex = surfelId;
  }
