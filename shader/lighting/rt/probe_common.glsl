#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

const uint UNUSED_BIT = 0x1;
const uint TRACED_BIT = 0x2;
const uint REUSE_BIT  = 0x4;
const uint BAD_BIT    = 0x8;
const uint NEW_BIT    = 0x10; //for debug view

struct ProbesHeader { // 64 bytes
  uint count;
  uint iterator;
  uint iterator2;
  uint tracedCount;
  uint watch; //debug
  uint padd1[11];
  };

struct Probe { // 32 bytes
  vec3 pos;
  uint pNext;
  vec3 normal;
  uint bits;
  };

const float dbgViewRadius    = 3;
const float probeGridStep    = 25;
const float probeCageBias    = 2.5;
const float probeBadHitT     = 25/5.0;
const float probeRayDistance = 200*100; // Lumen rt-probe uses 200-meters range
const float SKY_DEPTH        = 0.999995;

ivec2 gbufferCoord(const uint probeId, const uint sampleId) {
  uint x = (probeId     ) & 0xFF;
  uint y = (probeId >> 8);
  x = (x << 4) + ((sampleId     ) & 0xF);
  y = (y << 4) + ((sampleId >> 4) & 0xF);
  return ivec2(x,y);
  }

ivec2 lightBufferCoord(const uint probeId) {
  uint x = (probeId     ) & 0xFF;
  uint y = (probeId >> 8) & 0xFF;
  x = (x * 3);
  y = (y * 2);
  return ivec2(x,y);
  }

uint probeGridPosHash(ivec3 gridPos) {
  return (gridPos.x * 18397) + (gridPos.y * 20483) + (gridPos.z * 29303);
  }

vec3 probeReadAmbient(in sampler2D irradiance, uint id, vec3 nx, vec3 probeNorm) {
  ivec2 uv  = lightBufferCoord(id);
  ivec3 d;

  vec3  n = nx;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = n*n;

  vec3 ret = vec3(0);
  ret += texelFetch(irradiance, uv + ivec2(0,d.x), 0).rgb * n.x;
  ret += texelFetch(irradiance, uv + ivec2(1,d.y), 0).rgb * n.y;
  ret += texelFetch(irradiance, uv + ivec2(2,d.z), 0).rgb * n.z;
  return ret;
  }

int probeGridComputeLod(ivec2 fragCoord, ivec2 screenSize, float z, mat4 viewProjectInv, float bias) {
  const vec2  inPosL = vec2(2*(fragCoord+ivec2(-1,0))+ivec2(1,1))/vec2(screenSize) - vec2(1,1);
  const vec2  inPosR = vec2(2*(fragCoord+ivec2(+1,0))+ivec2(1,1))/vec2(screenSize) - vec2(1,1);

  const vec4  posL   = viewProjectInv*vec4(inPosL.xy,z,1);
  const vec4  posR   = viewProjectInv*vec4(inPosR.xy,z,1);

  const float dp     = distance(posL.xyz/posL.w, posR.xyz/posR.w)/probeGridStep;

  const int ilog     = max(0, int(bias + log2(dp)));
  return int(ilog);
  }

bool probeIsSame(vec3 ppos, vec3 expectedPos, int lod) {
  // NOTE: reconstructed position may jitter a little
  vec3 dp = (ppos - expectedPos);
  if(dot(dp,dp)>1.0)
    return false;
  return true;
  // const float step = probeGridStep*(1 << lod);
  // const vec3  dp   = abs(ppos - expectedPos);
  // return all(lessThan(dp, vec3(step)));
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

bool probeQueryProceed(inout probeQuery q, int id) {
  q.iterator = id;
  return probeQueryProceed(q);
  }

vec3 probeQueryWorldPos(in probeQuery q) {
  return q.px*q.step;
  }

ivec3 probeQueryGridPos(in probeQuery q) {
  return q.px*(1 << q.lod);
  }

#endif
