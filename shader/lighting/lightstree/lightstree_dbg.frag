#version 460

#include "lighting/lightstree/lightstree_common.glsl"
#include "scene.glsl"
#include "common.glsl"
#include "random.glsl"

#define ALL_LIGHTS

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
  BVHNode node[];
  } bvhData;

vec3 traverseLightTree(const vec3 wpos, const vec3 norm, inout Random rng) {
  vec3 ret = vec3(0);

  uint stack[64];
  uint ptr = 0;

  uint node = 0 | BVH_BoxNode;
  while(true) {
    if(ptr==stack.length()) {
      // stack overflow
      return vec3(1,0,0);
      }

    const uint    type = bvhGetNodeType(node);
    const BVHNode n    = bvhData.node[node & 0x0FFFFFFF];

    if(type==BVH_LightNode) {
      // light
      const vec4  src       = n.rmin;
      const vec3  distance  = wpos - src.xyz;
      const float tMax      = length(distance);
      const vec3  ldir      = distance/tMax;
      const float intensity = lightIntensity(norm, tMax, ldir, src.w);
      ret += intensity * n.rmax.rgb;
      }
    else {
      const bool left  = bvhIntersectBox(wpos, n.lmin.xyz, n.lmax.xyz);
      const bool right = bvhIntersectBox(wpos, n.rmin.xyz, n.rmax.xyz);
#if defined(ALL_LIGHTS)
      if(left && right) {
        node         = floatBitsToUint(n.lmin.w);
        stack[ptr++] = floatBitsToUint(n.rmin.w);
        continue;
        }
      else if(left) {
        node = floatBitsToUint(n.lmin.w);
        continue;
        }
      else if(right) {
        node = floatBitsToUint(n.rmin.w);
        continue;
        }
#else
      if(left && right) {
        node = randf(rng) < 0.5 ? floatBitsToUint(n.lmin.w) : floatBitsToUint(n.rmin.w);
        continue;
        }
      else if(left) {
        node = floatBitsToUint(n.lmin.w);
        continue;
        }
      else if(right) {
        node = floatBitsToUint(n.rmin.w);
        continue;
        }
#endif
      }
#if defined(ALL_LIGHTS)
    if(ptr == 0)
      break;
    node = stack[--ptr];
#else
    break;
#endif
    }

  return ret;
  }

void main(void) {
  const float d = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r;
  if(d>=1.0)
    discard;

  const vec3 norm = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));
  const vec4 pos4 = scene.viewProjectLwcInv*vec4(inUV * 2.0 - 1.0, d, 1);

  vec3   wpos = pos4.xyz/pos4.w + originLwc;
  Random rng  = srand(uvec2(gl_FragCoord.xy), 0);
  vec3   clr  = traverseLightTree(wpos, norm, rng) * max(1.0, scene.exposure);

  outColor = vec4(clr, 1);
  }
