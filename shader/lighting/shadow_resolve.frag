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

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(binding = 1) uniform sampler2D diffuse;
layout(binding = 2) uniform sampler2D normals;
layout(binding = 3) uniform sampler2D depth;

#if defined(SHADOW_MAP)
layout(binding = 4) uniform sampler2D textureSm0;
layout(binding = 5) uniform sampler2D textureSm1;
#endif

float lambert(vec3 normal) {
  return max(0.0, dot(scene.sunDir,normal));
  }

vec4 worldPos(ivec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectInv * scr;
  }

vec4 worldPosLwc(ivec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectLwcInv * scr;
  }

vec4 worldPos(float depth) {
  return worldPos(ivec2(gl_FragCoord.xy),depth);
  }

#if defined(SHADOW_MAP)
float calcShadow(vec4 pos4, float bias) {
  return calcShadow(pos4, bias, scene, textureSm0, textureSm1);
  }
#else
float calcShadow(vec4 pos4, float bias) { return 1.0; }
#endif

#if defined(RAY_QUERY)
bool rayTest(vec3 pos, vec3  rayDirection, float tMin, float tMax, out float rayT) {
#if defined(RAY_QUERY_AT)
  uint flags = gl_RayFlagsCullBackFacingTrianglesEXT;
#else
  uint flags = gl_RayFlagsOpaqueEXT;
#endif

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
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

float calcRayShadow(vec4 pos4, vec3 normal, float depth) {
  vec4 sp = scene.viewShadow[1]*pos4;
  if(all(lessThan(abs(sp.xy)+0.001,vec2(abs(sp.w)))))
    return 1.0;

  // NOTE: shadow is still leaking! Need to develop pso.depthClampEnable to fix it.
  vec4 cam4 = worldPos(0.1);
  vec3 cam  = cam4.xyz/cam4.w;
  vec3 pos  = pos4.xyz/pos4.w;

  // fine depth. 32F depth almost works, but still has self hadowing artifacts
  float fineDepth = 0;
  vec3  view      = pos - cam;
  float viewL     = length(view);
  rayTest(cam, view/viewL, max(viewL-200.0, 0), viewL+1000.0, fineDepth);

  pos = cam + view*(fineDepth/viewL);

  // float tMin = 50; // bias?
  if(!rayTest(pos, 0.01, 10000))
    return 1.0;
  return 0.0;
  }
#endif

vec3 flatNormal(vec4 pos4) {
  vec3 pos = pos4.xyz/pos4.w;
  vec3 dx  = dFdx(pos);
  vec3 dy  = dFdy(pos);
  return (cross(dx,dy));
  }

void main(void) {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const float d         = texelFetch(depth, fragCoord, 0).r;
  if(d==1.0) {
    outColor = vec4(0);
    return;
    }

  const vec4  diff   = texelFetch(diffuse, fragCoord, 0);
  const vec4  nrm    = texelFetch(normals, fragCoord, 0);
  const vec3  normal = normalize(nrm.xyz*2.0-vec3(1.0));

  bool isFlat  = false;
  bool isATest = false;
  bool isWater = false;
  decodeBits(diff.a, isFlat, isATest, isWater);

  const float light  = (isFlat ? 0 : lambert(normal));

  float shadow = 1;
  if(light>0) {
    if(dot(scene.sunDir,normal)<=0)
      shadow = 0;

#if defined(LWC)
    const vec4 wpos  = worldPosLwc(ivec2(gl_FragCoord.xy),d);
#else
    const vec4 wpos  = worldPos(d);
#endif

    shadow = calcShadow(wpos,(isATest ? 8 : -1));
#if defined(RAY_QUERY)
    if(shadow>0.01) {
      const vec4 wpos = worldPos(d);
      shadow *= calcRayShadow(wpos,normal,d);
      }
#endif
    }

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

  const vec3 lcolor = scene.sunCl.rgb * scene.GSunIntensity * Fd_Lambert * light * shadow;
  const vec3 linear = textureLinear(diff.rgb);

  vec3 color = linear*lcolor*scene.exposure;
  color *= (1.0/M_PI); // magic constant, non motivated by physics
  outColor = vec4(color, 0.0);

  // outColor = vec4(vec3(lcolor), diff.a); // debug
  // if(diff.a>0)
  //   outColor.r = 0;
  }
