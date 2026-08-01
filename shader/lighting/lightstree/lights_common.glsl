#ifndef LIGHTSTREE_COMMON_GLSL
#define LIGHTSTREE_COMMON_GLSL

#include "common.glsl"

const uint BVH_NullNode  = 0x00000000;
const uint BVH_BoxNode   = 0x10000000;
const uint BVH_LightNode = 0x40000000;

struct PTreeNode {
  vec3  centerL; float weightL;
  uvec3 padd0;   uint  ptrL;
  vec3  centerR; float weightR;
  uvec3 padd1;   uint  ptrR;
  };

struct BVHNode {
  vec3 lmin; uint padd0;
  vec3 lmax; uint ptrL;
  vec3 rmin; uint padd1;
  vec3 rmax; uint ptrR;
  };

uint bvhGetNodeType(uint ptr) {
  return ptr & 0xF0000000;
  }

bool bvhIntersectBox(vec3 point, vec3 lo, vec3 hi){
  return all(lessThanEqual(lo, point)) && all(lessThanEqual(point, hi));
  }

float grayscale(vec3 color) {
  return dot(color, vec3(0.2125, 0.7154, 0.0721));
  }

float bvhLightsNodeWeight(const vec3 wpos, const vec3 norm, const vec3 cen, float wFlux) {
  vec3  dlen                = wpos - cen;
  float distSq              = max(dot(dlen, dlen), 0.0001f);
  float distanceAttenuation = 1.0 / distSq;
  return distanceAttenuation * wFlux;
  }

float squareFalloffAttenuation(const float distance, const float lightInvRadius) {
  float distanceSquare = (distance * distance);
  float factor         = distanceSquare * lightInvRadius * lightInvRadius;
  float smoothFactor   = max(1.0 - factor * factor, 0.0);
  return (smoothFactor * smoothFactor) / max(factor, 0.005);
  }

//FIXME: copypaste of RTSM; duplicate if lights.frag logic
float lightIntensity(const vec3 normal, const float distance, const vec3 ldir, const float lightRange) {
  if(distance>lightRange)
    return 0;

  float falloff = squareFalloffAttenuation(distance, 1.0/lightRange);
  float lambert = max(0.0,-dot(ldir,normal));

  return lambert * falloff * 0.25;
  }

#endif
