#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define LWC 1

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "lighting/shadow_sampling.glsl"
#include "lighting/purkinje_shift.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(push_constant, std430) uniform UboPush {
  vec3  originLwc;
  } push;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(binding = 1) uniform sampler2D  gbufDiffuse;
layout(binding = 2) uniform usampler2D gbufNormal;
layout(binding = 3) uniform sampler2D  depth;

#if defined(SHADOW_MAP)
layout(binding = 4) uniform sampler2D textureSm0;
layout(binding = 5) uniform sampler2D textureSm1;
#endif

bool planetOcclusion(float viewPos, vec3 sunDir) {
  const float y = RPlanet + max(viewPos*0.1, 0);
  if(rayIntersect(vec3(0,y,0), sunDir, RPlanet)>=0)
    return true;
  return false;
  }

vec4 worldPos(vec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectInv * scr;
  }

vec4 worldPosLwc(vec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectLwcInv * scr;
  }

vec4 worldPos(float depth) {
  return worldPos(gl_FragCoord.xy,depth);
  }

#if defined(SHADOW_MAP)
float calcShadow(vec3 pos, const vec3 normal) {
  return calcShadow(pos, normal, scene, textureSm0, textureSm1);
  }
#else
float calcShadow(vec3 pos, const vec3 normal) { return 1.0; }
#endif

#if defined(RAY_QUERY)
bool rayTest(vec3 pos, vec3  rayDirection, float tMin, float tMax, out float rayT) {
  uint flags = RayFlagsShadow;
#if !defined(RAY_QUERY_AT)
  flags |= gl_RayFlagsOpaqueEXT;
#endif

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, CM_ShadowCaster,
                        pos, tMin, rayDirection, tMax);
  rayQueryProceedShadow(rayQuery);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true)==gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  rayT = rayQueryGetIntersectionTEXT(rayQuery, true);
  return true;
  }

bool rayTest(vec3 pos, float tMin, float tMax) {
  float t = 0;
  return rayTest(pos, scene.sunDir, tMin, tMax, t);
  }

float calcRayShadow(vec3 pos, vec3 normal, float depth) {
  vec4 sp = scene.viewShadowLwc[1]*vec4(pos,1);
  if(all(lessThan(abs(sp.xy/sp.w), vec2(0.999))))
    return 1.0;

  const float tMin = 5;
  if(!rayTest(pos + push.originLwc, tMin, 5000*100))
    return 1.0;
  return 0.0;
  }
#endif

float shadowFactor(const float depth, const vec3 normal, bool isFlat) {
  float light  = (isFlat ? 0 : dot(scene.sunDir,normal));
  if(light<=0)
    return 0;

  const float NormalBias = 0.0015;
  const vec4  wpos4 = worldPosLwc(gl_FragCoord.xy, depth) + vec4(normal*NormalBias, 0);
  const vec3  wpos  = wpos4.xyz/wpos4.w;

  if(planetOcclusion(wpos.y, scene.sunDir))
    return 0;

  light *= calcShadow(wpos, normal);

#if defined(RAY_QUERY)
    if(light>0.001) {
      light *= calcRayShadow(wpos, normal, depth);
      }
#endif

  return light;
  }

void main(void) {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const float d         = texelFetch(depth, fragCoord, 0).r;
  if(d==1.0) {
    outColor = vec4(0);
    return;
    }

  const vec4  diff   = texelFetch(gbufDiffuse, fragCoord, 0);
  const vec3  normal = normalFetch(gbufNormal, fragCoord);

  const bool  isFlat = isGBufFlat(diff.a);
  const float light  = shadowFactor(d, normal, isFlat);

#if defined(SHADOW_MAP)
  /*
  {
    // debug code
    const vec4 pos4  = worldPos(d);
    const vec4 shPos = scene.viewShadow[1]*vec4(pos4);
    // vec4 s = shadowSample(textureSm1, shPos.xy/shPos.w);
    // shadow = s.x;
    outColor = vec4(0, (isFlat ? 1 : 0), (isATest ? 1 : 0), 1.0);
    return;
  }
  */
#endif

  const vec3 illuminance = scene.sunColor * light;
  const vec3 linear      = textureAlbedo(diff.rgb);

  const vec3 luminance   = linear * Fd_Lambert * illuminance;

  outColor = vec4(luminance * scene.exposure, 0.0);

  // outColor = vec4(vec3(lcolor), diff.a); // debug
  // if(diff.a>0)
  //   outColor.r = 0;
  }
