#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

struct Probe {
  vec3 pos;
  uint padd0;
  vec3 color[3][2];
  };

struct ProbesHeader {
  uint count;
  uint iterator;
  };

const float dbgViewRadius = 5;
const float probeGridStep = 50;

uint probePositionHash(ivec3 gridPos) {
  return (gridPos.x * 18397) + (gridPos.y * 20483) + (gridPos.z * 29303);
  }

#endif
