#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

struct Hash {
  uint value;
  };

const uint UNUSED_BIT = 0x1;
const uint TRACED_BIT = 0x2;
const uint REUSE_BIT  = 0x4;
const uint BAD_BIT    = 0x8;
const uint NEW_BIT    = 0x10; //for debug view

struct Probe {
  vec3 pos;
  uint bits;
  vec3 color[3][2];  // HL2-cube
  //uvec2 gbuffer[128]; // x: normal[xy]; w - color[rgb]; normal[z]
  };

struct ProbesHeader {
  uint count;
  uint iterator;
  uint tracedCount;
  };

const float dbgViewRadius = 5;
const float probeGridStep = 50;

uint probeGridPosHash(ivec3 gridPos) {
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

int  probeGridLodFromDist(const float depth) {
  // NOTE: manual tuning been here
  if(depth < 0.25)
    return 0;
  if(depth < 0.5)
    return 0;
  if(depth <= 1)
    return 0;
  if(depth <= 1.5)
    return 0;
  if(depth <= 6.0)
    return 1;
  if(depth <= 9.0)
    return 2;
  return 3;
  }


struct probeQuery {
  int   lod;
  float step;
  ivec3 pLow, pHigh;

  int   iterator;
  ivec3 px;
  };

void probeQueryInitialize(out probeQuery q, vec3 pos, int lod) {
  const float step    = probeGridStep*(1 << lod);
  const vec3  gridPos = pos/step;

  q.pLow     = ivec3(floor(gridPos));
  q.pHigh    = ivec3(ceil (gridPos));
  q.iterator = 0;
  q.step     = step;
  q.lod      = lod;
  }

bool probeQueryProceed(inout probeQuery q) {
  bool x = (q.iterator & 0x1)!=0;
  bool y = (q.iterator & 0x2)!=0;
  bool z = (q.iterator & 0x4)!=0;

  ivec3 px = ivec3(x ? q.pLow.x : q.pHigh.x,
                   y ? q.pLow.y : q.pHigh.y,
                   z ? q.pLow.z : q.pHigh.z);

  q.px      = px;
  q.iterator++;
  return q.iterator<=8;
  }

vec3 probeQueryWorldPos(in probeQuery q) {
  return q.px*q.step;
  }

ivec3 probeQueryGridPos(in probeQuery q) {
  return q.px*(1 << q.lod);
  }

#endif
