#ifndef SURF_COMMON_GLSL
#define SURF_COMMON_GLSL

#include "common.glsl"
#include "scene.glsl"

const float SKY_DEPTH       = 0.999995;
const int   MinCoverage     = 4;    // in pixels
const int   DefaultCoverage = 96;   // in pixels
const int   LargeTile       = 128;  // in pixels
const uint  MaxInTile       = 1024; // ~32px (~6x6) per surfel
const ivec2 GBufTile        = ivec2(8);

const float rEffScale       = 0.5;

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
  uint  payload;
  };

struct Candidate {
  vec4  pos;  // pos,  size
  vec4  norm; // norm, padd
  };

uint packAtlassPos(uint x, uint y) {
  return (x) | (y << 12) | 0x80000000;
  }

ivec2 unpackAtlassPos(uint ptr) {
  uint x = ((ptr >>  0) & 0xFFF);
  uint y = ((ptr >> 12) & 0xFFF);
  return ivec2(x, y);
  }

bool isSurfelAlive(const Surfel s) {
  return s.radius >= 0;
  }

bool isSurfelVisible(const Surfel s, ivec2 bboxMin, ivec2 bboxMax) {
  const ivec2 at     = s.fragCoord;
  const int   radius = DefaultCoverage;

  if(bboxMax.x < at.x-radius || bboxMax.y < at.y-radius)
    return false;
  if(at.x+radius < bboxMin.x || at.y+radius < bboxMin.y)
    return false;
  return true;
  }

bool planetOcclusion(float viewPos, vec3 sunDir) {
  const float y = RPlanet + max(viewPos*0.1, 0);
  if(rayIntersect(vec3(0,y,0), sunDir, RPlanet)>=0)
    return true;
  return false;
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

float pixelToWorld(const SceneDesc scene, float pixelRadius, float z) {
  z = linearDepth(z, scene.clipInfo);

  float clipRadiusX  = (2.0 * pixelRadius) * scene.screenResInv.x;
  float clipRadiusY  = (2.0 * pixelRadius) * scene.screenResInv.y;

  float worldRadiusX = (clipRadiusX * z) / scene.project[0][0];
  float worldRadiusY = (clipRadiusY * z) / scene.project[1][1];
  return min(worldRadiusX, worldRadiusY);
  }

float calculteWeight(const vec3 spos, const vec3 snorm, float rEff, float rMax, float rDiffInv, const vec3 wpos, const vec3 wnorm) {
  // An Approximate Global Illumination System for Computer Generated Films
  // https://www.tabellion.org/et/paper/siggraph_2004_gi_for_films.pdf
  // https://cgg.mff.cuni.cz/~jaroslav/papers/2008-irradiance_caching_class/03-greg-ic.pdf
  float dotN  = dot(wnorm, snorm);
  if(dotN <= 0)
    return 0;

  vec3  ldir  = wpos - spos;
  float dist  = length(ldir);

  dist = max(dist, 0.0001);
  // Wendland C2 inspired falloff
  float q     = max(min(dist,rMax)-rEff, 0)*rDiffInv;
  float wPos  = pow(1-q, 4.0)*(4.0*q + 1.0);
  float wNorm = dotN * dotN;
  float wOccl = 1.0 - max(dot(ldir, snorm)/dist, 0.0);
  return wPos * wNorm * wOccl;
  }

float calculteWeight(const vec3 spos, const vec3 snorm, float rEff, float rMax, const vec3 wpos, const vec3 wnorm) {
  rMax  = min(rMax, 65000);
  rEff  = min(rEff, rMax*rEffScale);
  return calculteWeight(spos, snorm, rEff, rMax, 1.0/(rMax-rEff), wpos, wnorm);
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