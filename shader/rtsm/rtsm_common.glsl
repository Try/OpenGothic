#ifndef RTSM_COMMON_GLSL
#define RTSM_COMMON_GLSL

#include "common.glsl"

#define DEBUG_IMG 1

const uint NULL               = 0;

const int  RTSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int  RTSM_PAGE_MIPS     = 16;

const int  RTSM_LARGE_TILE    = 128;
const int  RTSM_SMALL_TILE    = 32;
const int  RTSM_BIN_SIZE      = 32;
const int  RTSM_LIGHT_TILE    = 64;

const uint MaxSlices          = 16;
const uint MaxVert            = 64;
const uint MaxPrim            = 64;
const uint MaxInd             = (MaxPrim*3);

const float NormalBias        = 0.0015;

struct RtsmHeader {
  uint visCount;
  uint one1;
  uint one2;
  };

struct LightId {
  uint id;
  uint aabb_low;
  uint aabb_high;
  uint ptr;
  };

// utility
uint floatToOrderedUint(float value) {
  uint uvalue = floatBitsToUint(value);
  uint mask = -int(uvalue >> 31) | 0x80000000;
  return uvalue ^ mask;
  }

float orderedUintToFloat(uint value) {
  uint mask = ((value >> 31) - 1) | 0x80000000;
  return uintBitsToFloat(value ^ mask);
  }

vec3 orderedUintToFloat(uvec3 value) {
  vec3 r;
  r.x = orderedUintToFloat(value.x);
  r.y = orderedUintToFloat(value.y);
  r.z = orderedUintToFloat(value.z);
  return r;
  }

vec4 orderedUintToFloat(uvec4 value) {
  vec4 r;
  r.x = orderedUintToFloat(value.x);
  r.y = orderedUintToFloat(value.y);
  r.z = orderedUintToFloat(value.z);
  r.w = orderedUintToFloat(value.w);
  return r;
  }

float edgeFunction(const vec2 a, const vec2 b, const vec2 c) {
  return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
  }

vec4 bboxOf(vec2 a, vec2 b, vec2 c) {
  vec4 ret;
  ret.x = min(a.x, min(b.x, c.x));
  ret.z = max(a.x, max(b.x, c.x));
  ret.y = min(a.y, min(b.y, c.y));
  ret.w = max(a.y, max(b.y, c.y));
  return ret;
  }

bool bboxIntersect(vec4 a, vec4 b) {
  if(b.z < a.x || b.x > a.z)
    return false;
  if(b.w < a.y || b.y > a.w)
    return false;
  return true;
  }

bool bboxIncludes(vec4 aabb, vec2 p) {
  if(aabb.x < p.x && p.x < aabb.z &&
     aabb.y < p.y && p.y < aabb.w)
    return true;
  return false;
  }

//
bool planetOcclusion(float viewPos, vec3 sunDir) {
  const float y = RPlanet + max(viewPos*0.1, 0);
  if(rayIntersect(vec3(0,y,0), sunDir, RPlanet)>=0)
    return true;
  return false;
  }

// primitive-list
uint packMeshHeader(uint bucketId, uint primCnt) {
  return (bucketId << 8) | (primCnt & 0xFF);
  }

uint unpackPrimitiveCount(uint v) {
  return v & 0xFF;
  }

uint unpackBucketId(uint v) {
  return v >> 8;
  }

// omni-lights
uint rayToFace(vec3 d) {
  const vec3 ad = abs(d);
  if(ad.x > ad.y && ad.x > ad.z)
    return d.x>=0 ? 0 : 1;
  if(ad.y > ad.x && ad.y > ad.z)
    return d.y>=0 ? 2 : 3;
  if(ad.z > ad.x && ad.z > ad.y)
    return d.z>=0 ? 4 : 5;
  return 0;
  }

vec3 rayToFace(vec3 pos, uint face) {
  // cubemap-face
  switch(face) {
    case 0: pos = vec3(pos.yz, +pos.x); break;
    case 1: pos = vec3(pos.zy, -pos.x); break;
    case 2: pos = vec3(pos.zx, +pos.y); break;
    case 3: pos = vec3(pos.xz, -pos.y); break;
    case 4: pos = vec3(pos.xy, +pos.z); break;
    case 5: pos = vec3(pos.yx, -pos.z); break;
    }
  pos.xy /= pos.z;
  return pos;
  }

vec3 faceToRay(vec2 xy, uint face) {
  vec3 pos = normalize(vec3(xy, 1.0));
  switch(face) {
    case 0: pos = vec3(+pos.z, pos.xy); break;
    case 1: pos = vec3(-pos.z, pos.yx); break;
    case 2: pos = vec3(pos.y, +pos.z, pos.x); break;
    case 3: pos = vec3(pos.x, -pos.z, pos.y); break;
    case 4: pos = vec3(pos.xy, +pos.z); break;
    case 5: pos = vec3(pos.yx, -pos.z); break;
    }
  return pos;
  }

#endif
