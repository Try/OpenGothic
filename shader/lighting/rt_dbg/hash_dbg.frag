#version 460

#include "lighting/surfels/surf_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(std140, push_constant) uniform Push {
  vec3 originLwc;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
//layout(binding = 1) uniform texture2D depth;
layout(binding = 2) uniform usampler2D gbufNormal;
layout(binding = 3) uniform texture2D  depth;

uint computeDepthSlice(float linDepth, float near, float far, uint numSlices) {
  float z = linDepth;
  // Compute logarithmic slice index
  float slice_float = log(z / near) * (float(numSlices) / log(far / near));
  // Clamp to ensure it doesn't exceed the grid bounds
  return clamp(uint(slice_float), 0u, numSlices - 1u);
  }

ivec3 hasGridPos(vec3 wpos, float cellSize) {
  wpos = wpos / cellSize;
  return ivec3(round(wpos));
  }

float surfDist(ivec3 hpos, vec3 pos, float cellSize) {
  vec3 p = (pos/cellSize - hpos);
  p *= 2/sqrt(2.0);
  return dot(p,p); //quad distance
  }

uint surfHash(ivec3 hpos) {
  ivec3 p       = hpos;
  uint  hashKey = pcgHash(p.x + pcgHash(p.y + pcgHash(p.z)));
  return hashKey;
  }

void main(void) {
  const float d = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r;
  if(d>=1.0)
    discard;

  const vec3 norm = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));

  const float lD  = max(linearDepth(d, scene.clipInfo), 10);
  const mat4  inv = scene.viewProjectLwcInv;
  const vec4  pos = inv*vec4(inUV * 2.0 - 1.0, d, 1);

  float fov      = 67.5f*M_PI/180.0;
  float cellSize = 25;

  // float sz  = computeCellSize(lD, fov, textureSize(depth,0), 16, cellSize);
  // float sz  = computeCellSize(lD, fov, textureSize(depth,0), 2, 1);
  float sz   = computeCellSize(lD, fov, textureSize(depth,0), 64);
  vec3  wpos = pos.xyz/pos.w + originLwc;

  ivec3 ipos  = hasGridPos(wpos, sz);
  uint  h     = surfHash(ipos);
  float dx    = surfDist(ipos, wpos, sz);

  //uint  sx  = computeDepthSlice(lD, 10, 100000, 32);

  // outColor = vec4(debugColors[h%debugColors.length()], 1.0);
  // outColor = vec4(dx * debugColors[h%debugColors.length()], 1.0);
  outColor = vec4(dx);
  // outColor = vec4(debugColors[sx%debugColors.length()], 1.0);
  }
