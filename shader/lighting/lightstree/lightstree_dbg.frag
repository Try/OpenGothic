#version 460

#define LIGHTS_BVH

#include "lighting/lightstree/lights_common.glsl"
#include "scene.glsl"
#include "common.glsl"
#include "random.glsl"

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(std140, push_constant) uniform Push {
  vec3 originLwc;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform usampler2D gbufNormal;
layout(binding = 2) uniform texture2D  depth;
layout(binding = 3, std430) readonly buffer BVH {
#if defined(LIGHTS_BVH)
  BVHNode   node[];
#else
  PTreeNode node[];
#endif
  } bvhData;

#if !defined(LIGHTS_BVH)
vec3 traverseLightTree(const vec3 wpos, const vec3 norm, inout Random rng) {
  float pdf  = 1.0;
  float key  = randf(rng);
  uint  node = 0 | BVH_BoxNode;

  while(node!=0) {
    const uint      type = bvhGetNodeType(node);
    const PTreeNode n    = bvhData.node[node & 0x0FFFFFFF];

    if(type==BVH_LightNode) {
      // light
      const vec3  distance  = wpos - n.centerL;
      const float tMax      = length(distance);
      const vec3  ldir      = distance/tMax;
      const float intensity = lightIntensity(norm, tMax, ldir, n.weightR);
      return intensity * n.centerR;
      }

    const float wLeft  = bvhLightsNodeWeight(wpos, norm, n.centerL, n.weightL);
    const float wRight = bvhLightsNodeWeight(wpos, norm, n.centerR, n.weightR);
    const float pLeft  = wLeft /(wLeft + wRight);
    const float pRight = wRight/(wLeft + wRight);
    if(key < pLeft) {
      pdf *= pLeft;
      node = n.ptrL;
      key = key/pLeft;
      } else {
      pdf *= pRight;
      node = n.ptrR;
      key = (key-pLeft)/pRight;
      }
    }

  return vec3(0);
  }
#endif

#if defined(LIGHTS_BVH)
vec3 traverseLightBvh(const vec3 wpos, const vec3 norm) {
  uint stack[32];
  uint ptr = 0;

  vec3 ret  = vec3(0);
  uint node = 0 | BVH_BoxNode;
  while(node!=0) {
    const uint    type = bvhGetNodeType(node);
    const BVHNode n    = bvhData.node[node & 0x0FFFFFFF];

    if(ptr==stack.length()) {
      // stack overflow
      return vec3(1,0,0);
      }

    if(type==BVH_LightNode) {
      // light
      const vec3  src       = n.lmin;
      const vec3  distance  = wpos - src;
      const float tMax      = length(distance);
      const vec3  ldir      = distance/tMax;
      const float intensity = lightIntensity(norm, tMax, ldir, uintBitsToFloat(n.ptrL));
      ret += intensity * n.lmax.rgb;
      }
    else {
      const bool left  = bvhIntersectBox(wpos, n.lmin, n.lmax);
      const bool right = bvhIntersectBox(wpos, n.rmin, n.rmax);
      if(left && right) {
        node         = n.ptrL;
        stack[ptr++] = n.ptrR;
        continue;
        }
      else if(left) {
        node = n.ptrL;
        continue;
        }
      else if(right) {
        node = n.ptrR;
        continue;
        }
      }
    if(ptr == 0)
      break;
    node = stack[--ptr];
    }

  return ret;
  }
#endif

void main() {
  const float d = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r;
  if(d>=1.0)
    discard;

  const vec3 norm = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));
  const vec4 pos4 = scene.viewProjectLwcInv*vec4(inUV * 2.0 - 1.0, d, 1);

  vec3   wpos = pos4.xyz/pos4.w + originLwc;
#if defined(LIGHTS_BVH)
  vec3   clr  = traverseLightBvh(wpos, norm);
#else
  Random rng  = srand(uvec2(gl_FragCoord.xy), 0);
  vec3   clr  = traverseLightTree(wpos, norm, rng);
#endif
  outColor = vec4(clr * max(1.0, scene.exposure) * Fd_Lambert, 1);
  }
