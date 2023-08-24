#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

struct Probe {
  vec3 pos;
  uint badbit;
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

vec3 probeReadAmbient(const in Probe p, vec3 n) {
  ivec3 d;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = n*n;

  vec3 ret = vec3(0);
  ret += p.color[0][d.x].rgb * n.x;
  ret += p.color[1][d.y].rgb * n.y;
  ret += p.color[2][d.z].rgb * n.z;
  return ret;
  }

#endif
