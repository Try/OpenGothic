#version 460

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
layout(binding = 1) uniform texture2D depth;

uint pcg(uint v) {
  uint state = v * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
  }

uint computeDepthSlice(float linDepth, float near, float far, uint numSlices) {
  float z = linDepth;
  // Compute logarithmic slice index
  float slice_float = log(z / near) * (float(numSlices) / log(far / near));
  // Clamp to ensure it doesn't exceed the grid bounds
  return clamp(uint(slice_float), 0u, numSlices - 1u);
  }

float computeTargetCellSize(float d, float aperture, vec2 resolution, float pixelFeatureSize) {
  // Equation 2: Evaluate the angular factor based on resolution aspect ratio
  float term1 = aperture / resolution.x;
  float term2 = (aperture * resolution.x) / (resolution.y * resolution.y);
  float maxFactor = max(term1, term2);

  // Compute target feature size in world space
  float sw = d * tan(maxFactor * pixelFeatureSize);
  return sw;
  }

// Computes the discretized cell size (s_wd) rounding to the nearest power-of-two level
float computeAdaptiveCellSize(float sw, float smin) {
  // Avoid log2 of zero or negative numbers if sw is too small
  if(sw <= smin)
    return smin;

  // Equation 3: Discretize to power-of-two bands to create discrete levels of detail
  float logScale = floor(log2(sw / smin));
  float swd = pow(2.0, logScale) * smin;
  return swd;
  }

float computeCellSize(float d, float fov, vec2 resolution,
                      float pixelFeatureSize, float smin) {
  float sw = computeTargetCellSize(d, fov, resolution, pixelFeatureSize);
  return computeAdaptiveCellSize(sw, smin);
  }

ivec3 hasGridPos(vec3 wpos, float cellSize) {
  wpos = wpos / cellSize;
  return ivec3(round(wpos));
  }

float surfDist(ivec3 hpos, vec3 pos, float cellSize) {
  vec3 p = (pos/cellSize - hpos);
  p *= 2;///sqrt(2.0);
  return dot(p,p); //quad distance
  }

uint surfHash(ivec3 hpos) {
  ivec3 p       = hpos;
  uint  hashKey = pcg(p.x + pcg(p.y + pcg(p.z)));
  return hashKey;
  }

void main(void) {
  const float d = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r;
  if(d>=1.0)
    discard;

  const float lD  = max(linearDepth(d, scene.clipInfo), 10);
  const mat4  inv = scene.viewProjectLwcInv;
  const vec4  pos = inv*vec4(inUV * 2.0 - 1.0, d, 1);

  float fov      = 67.5f*M_PI/180.0;
  float cellSize = 25;

  // float sz  = computeCellSize(lD, fov, textureSize(depth,0), 16, cellSize);
  // float sz  = computeCellSize(lD, fov, textureSize(depth,0), 2, 1);
  float sz   = computeCellSize(lD, fov, textureSize(depth,0), 32, 1);
  vec3  wpos = pos.xyz/pos.w + originLwc;
  ivec3 ipos = hasGridPos(wpos, sz);
  uint  h    = surfHash(ipos);
  float dx   = surfDist(ipos, wpos, sz);
  //uint  sx  = computeDepthSlice(lD, 10, 100000, 32);

  // outColor = vec4(debugColors[h%debugColors.length()], 1.0);
  // outColor = vec4(dx * debugColors[h%debugColors.length()], 1.0);
  outColor = vec4(dx);
  // outColor = vec4(debugColors[sx%debugColors.length()], 1.0);
  }
