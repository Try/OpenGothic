#ifndef SURF_COMMON_GLSL
#define SURF_COMMON_GLSL

#include "common.glsl"

const float SKY_DEPTH        = 0.999995;
//const float SURFEL_FOOTPRINT = 64;
const float SURFEL_CELL      = 64;//SURFEL_FOOTPRINT/sqrt(2);

struct Surfel {
  vec3  pos;
  uint  norm;
  ivec2 fragCoord;
  float radius;
  int   radiusPix;
  };

uint octahedral_8(in vec3 nor) {
  nor.xy /= ( abs( nor.x ) + abs( nor.y ) + abs( nor.z ) );
  nor.xy  = (nor.z >= 0.0) ? nor.xy : (1.0-abs(nor.yx))*msign(nor.xy);
  uvec2 d = uvec2(round(7.5 + nor.xy*7.5));
  return d.x|(d.y<<4u);
  }

vec3 i_octahedral_8( uint data ) {
  uvec2 iv = uvec2( data, data>>4u ) & 15u; vec2 v = vec2(iv)/7.5 - 1.0;
  vec3 nor = vec3(v, 1.0 - abs(v.x) - abs(v.y)); // Rune Stubbe's version,
  float t = max(-nor.z,0.0);                     // much faster than original
  nor.x += (nor.x>0.0)?-t:t;                     // implementation of this
  nor.y += (nor.y>0.0)?-t:t;                     // technique
  return normalize( nor );
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
float computeAdaptiveCellSize(float sw, float sMin) {
  // Avoid log2 of zero or negative numbers if sw is too small
  if(sw <= sMin)
    return sMin;

  // Equation 3: Discretize to power-of-two bands to create discrete levels of detail
  float logScale = floor(log2(sw / sMin));
  float swd = pow(2.0, logScale) * sMin;
  return swd;
  }

float computeCellSize(float d, float fov, vec2 resolution, float pixelFeatureSize) {
  const float sMin = 1; // min 1 centimeter
  float sw = computeTargetCellSize(d, fov, resolution, pixelFeatureSize);
  return computeAdaptiveCellSize(sw, sMin);
  }

uint surfHash(vec3 pos, float cellSize, uint inorm) {
#if 1
  ivec3 p       = ivec3(round(pos * cellSize));
  uint  hashKey = pcgHash(inorm + pcgHash(p.x + pcgHash(p.y + pcgHash(p.z))));
  return hashKey;
#else
  ivec3 p = ivec3(round(pos * cellSize));

  uint hash = 0x811C9DC5;
  hash ^= p.x;
  hash *= 0x01000193; // FNV-1a prime
  hash = (hash << 13) | (hash >> 19);

  hash ^= p.y;
  hash *= 0x01000193;
  hash = (hash << 17) | (hash >> 15);

  hash ^= p.z;
  hash *= 0x01000193;

  // 5. Avalanche step to ensure every input bit affects every output bit
  hash ^= hash >> 16;
  hash *= 0x7feb352d;
  hash ^= hash >> 15;
  hash *= 0x846ca68b;
  hash ^= hash >> 16;

  return hash;
#endif
  }

#endif