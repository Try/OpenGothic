#ifndef SURF_COMMON_GLSL
#define SURF_COMMON_GLSL

const float SKY_DEPTH = 0.999995;

struct Surfel {
  vec3 pos;
  uint norm;
  };

uint pcg(uint v) {
  uint state = v * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
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

uint hash(vec3 pos, float cellSize) {
  ivec3 p      = ivec3(floor(pos / cellSize));
  uint hashKey = pcg(uint(cellSize) + pcg(p.x + pcg(p.y + pcg(p.z))));
  return hashKey;
  }

#endif