#ifndef SURF_COMMON_GLSL
#define SURF_COMMON_GLSL

#include "common.glsl"

const float SKY_DEPTH       = 0.999995;
const int   MinCoverage     = 8;   // in pixels
const int   DefaultCoverage = 128; // in pixels
const uint  MaxInTile       = 512; // ~32px (~6x6) per surfel

struct SurfHeader {
  uint count;
  uint one1;
  uint one2;
  uint added;
  };

struct Surfel {
  vec3  pos;
  uint  norm;
  ivec2 fragCoord;
  float radius;
  float radiusMean;
  vec3  irradiance;
  int   radiusPix;
  };

struct Candidate {
  vec4  pos;  // pos,  size
  vec4  norm; // norm, padd
  };

bool isSurfelVisible(const Surfel s, ivec2 bboxMin, ivec2 bboxMax) {
  const ivec2 at     = s.fragCoord;
  const int   radius = s.radiusPix;

  if(bboxMax.x < at.x-radius || bboxMax.y < at.y-radius)
    return false;
  if(at.x+radius < bboxMin.x || at.y+radius < bboxMin.y)
    return false;
  return true;
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

float calculteWeight(const vec3 spos, const vec3 snorm, float rEff, float rMax, const vec3 wpos, const vec3 wnorm) {
  // An Approximate Global Illumination System for Computer Generated Films
  // https://www.tabellion.org/et/paper/siggraph_2004_gi_for_films.pdf
  // https://cgg.mff.cuni.cz/~jaroslav/papers/2008-irradiance_caching_class/03-greg-ic.pdf
  vec3  ldir   = wpos - spos;
  float dist   = length(ldir);
  float dotN   = dot(wnorm, snorm);

  dist = max(dist, 0.0001);
#if 0
  float ePos  = dist/rEff;
  float eNorm = sqrt(max(1 - 1*dotN, 0)) / sqrt(1.0 - cos(M_PI/6.0)); // Eq. 4
  float w     = 1.0 - max(ePos, eNorm); // Eq. 2

  float eOccl = dot((ldir/dist), 0.5*(snorm+wnorm))*0.5+0.5; // allow small occlusion
  //float eOccl = 1.0 - clamp(-dot(ldir, wnorm), 0, 1)*0.5; // allow small occlusion
  //float eOccl = (dot(ldir, snorm) > 0.1*dist) ? 0.1 : 1;
  return w*eOccl;
#elif 1
  // Wendland C2 inspired falloff
  rEff = min(rEff, rMax*0.5);
  float q     = max(min(dist,rMax)-rEff, 0)/(rMax-rEff);
  float wPos  = pow(1-q, 4.0)*(4.0*q + 1.0);
  float wNorm = pow(max(dotN, 0.0), 2.0);
  float wOccl = 1.0 - max(dot((ldir/dist), snorm), 0.0);
  return wPos * wNorm * wOccl;
#elif 0
  float wPos  = 1.0 - smoothstep(min(rEff,rMax*0.6), rMax, dist);
  float wNorm = pow(max(dotN, 0.0), 2.0);
  float wOccl = 1.0 - max(dot((ldir/dist), snorm), 0.0);
  return wPos * wNorm * wOccl;
#elif 0
  float wPos  = max(1.0 - dist/rMax, 0.0)*(rEff/dist);
  float wNorm = pow(max(dotN, 0.0), 2.0);
  float wOccl = dot((ldir/dist), snorm)*0.5+0.5; // allow small occlusion
  return wPos * wNorm * wOccl;
#else
  float ePos  = max(dist/rEff, 0.0);
  float eNorm = sqrt(max(1 - 1*dotN, 0));
  float eOccl = dot((ldir/dist), 0.5*(snorm+wnorm))*0.5+0.5; // allow small occlusion
  return (1.0*eOccl)/max(ePos + eNorm, 0.0001) - 1.0;
#endif
  }

vec3 surfDebugColor(Surfel s, uint sId) {
  ivec3 p = ivec3(s.pos/10);
  uint  h = pcgHash(p.x + pcgHash(p.y + pcgHash(p.z)));
  return debugColors[h%debugColors.length()];
  }

vec3 surfDebugColor(vec3 pos, uint sId) {
  ivec3 p = ivec3(pos/10);
  uint  h = pcgHash(p.x + pcgHash(p.y + pcgHash(p.z)));
  return debugColors[h%debugColors.length()];
  }

#endif