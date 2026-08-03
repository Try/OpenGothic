#ifndef BVH_BUILD_COMMON_GLSL
#define BVH_BUILD_COMMON_GLSL

struct MortonHeader {
  uint numActiveLights;
  uint padd0, padd1, padd2;
  };

struct MortonPair {
  uint key;
  uint id;
  };

struct BVHAlux {
  uint parent;
  };

#endif