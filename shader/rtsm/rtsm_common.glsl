#ifndef RTSM_COMMON_GLSL
#define RTSM_COMMON_GLSL

#include "common.glsl"

const uint NULL               = 0;

const int  RTSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int  RTSM_PAGE_MIPS     = 16;

const int  RTSM_BIN_SIZE      = 32;
const int  RTSM_LARGE_TILE    = 128;
const int  RTSM_SMALL_TILE    = 32;

const uint MaxSlices          = 16;

struct RtsmHeader {
  uint visCount;
  uint one1;
  uint one2;
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
  if(rayIntersect(vec3(0,y,0), sunDir, RPlanet)>0)
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

#endif
