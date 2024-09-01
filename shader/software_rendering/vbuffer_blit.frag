#version 450

#extension GL_GOOGLE_include_directive          : enable
#extension GL_ARB_separate_shader_objects       : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"

layout(binding  = 0, r32ui) uniform uimage2D visBuffer;

layout(binding  = 1, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 2) uniform sampler2D  gbufDiffuse;
layout(binding  = 3) uniform usampler2D gbufNormal;
layout(binding  = 4) uniform sampler2D  gbufDepth;

layout(location = 0) out vec4 outColor;

vec3 upackColor(uint v) {
  uint r = (v >> 11u) & 0x1F;
  uint g = (v >> 5u)  & 0x3F;
  uint b = (v)        & 0x1F;
  return vec3(r, g, b)/vec3(31,63,31);
  }

vec3 rayTriangleTest(const vec3 origin, const vec3 raydir, const vec3 triA, const vec3 triB, const vec3 triC) {
  const float epsilon = 0.0001;

  vec3  edge1 = triB - triA;
  vec3  edge2 = triC - triA;
  vec3  ray_cross_e2 = cross(raydir, edge2);
  float det = dot(edge1, ray_cross_e2);

  if(det > -epsilon && det < epsilon)
    return vec3(-1);    // This ray is parallel to this triangle.

  float inv_det = 1.0 / det;
  vec3  s = origin - triA;
  float u = inv_det * dot(s, ray_cross_e2);

  if(u < 0 || u > 1)
    return vec3(-1);

  vec3 s_cross_e1 = cross(s, edge1);
  float v = inv_det * dot(raydir, s_cross_e1);

  if(v < 0 || u + v > 1)
    return vec3(-1);

  // At this stage we can compute t to find out where the intersection point is on the line.
  float t = inv_det * dot(edge2, s_cross_e1);

  if(t > epsilon) // ray intersection
    return vec3(t, u, v); //vec3(origin + raydir * t);

  // This means that there is a line intersection but not a ray intersection.
  return vec3(-1, u, v);
  }

void mainVBufferDecode() {
  uint v  = imageLoad(visBuffer, ivec2(gl_FragCoord.xy)).r;
  vec3 cl = upackColor(v & 0xFFFF);
  imageStore(visBuffer, ivec2(gl_FragCoord.xy), uvec4(0));

  outColor = (v>0 ? vec4(cl,1) : vec4(0));
  }

vec3 unproject(vec3 v) {
  vec4 vp = scene.viewProjectInv * vec4(v, 1);
  return vp.xyz/vp.w;
  }

void mainMapping() {
  const float dMax  = 0.9995;
  const float depth = texelFetch(gbufDepth, ivec2(gl_FragCoord.xy), 0).x;
  const vec3  at    = unproject(vec3((gl_FragCoord.xy/textureSize(gbufDepth, 0))*2.0-1.0, depth));

  vec3 a  = scene.camPos;
  vec3 LB = unproject(vec3(-1, 1, dMax));
  vec3 RB = unproject(vec3(+1, 1, dMax));
  vec3 LT = unproject(vec3(-1,-1, dMax));
  vec3 RT = unproject(vec3(+1,-1, dMax));

  vec3 tbc = vec3(0);
  tbc = rayTriangleTest(at, -scene.sunDir, a, LB, RB);
  if(tbc.x>=0) {
    outColor = vec4(tbc.yz, 1-tbc.y-tbc.z, 1);
    return;
    }

  tbc = rayTriangleTest(at, -scene.sunDir, a, LT, RT);
  if(tbc.x>=0) {
    outColor = vec4(tbc.yz, 1-tbc.y-tbc.z, 1);
    return;
    }

  tbc = rayTriangleTest(at, -scene.sunDir, a, LT, LB);
  if(tbc.x>=0) {
    outColor = vec4(tbc.yz, 1-tbc.y-tbc.z, 1);
    return;
    }

  tbc = rayTriangleTest(at, -scene.sunDir, a, RT, RB);
  if(tbc.x>=0) {
    outColor = vec4(tbc.yz, 1-tbc.y-tbc.z, 1);
    return;
    }
  }

void main() {
  outColor = vec4(0,0,0,0);
  mainVBufferDecode();
  // mainMapping();
  }
