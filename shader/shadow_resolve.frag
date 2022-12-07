#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "common.glsl"
#include "scene.glsl"
#include "lighting/shadow_sampling.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec2 UV;

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

#if defined(RAY_QUERY)
layout(binding = 6) uniform accelerationStructureEXT topLevelAS;
#endif

float lambert(vec3 normal) {
  return max(0.0, dot(scene.sunDir,normal));
  }

vec4 worldPos(ivec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectInv * scr;
  }

#if defined(SHADOW_MAP)
float calcShadow(vec4 pos4) {
  return calcShadow(pos4, scene, textureSm0, textureSm1);
  }
#else
float calcShadow(vec4 pos4) { return 1.0; }
#endif

#if defined(RAY_QUERY)
bool rayTest(vec3 pos, float tMin, float tMax) {
  vec3  rayDirection = scene.sunDir;

  // float tMin = 50;

  //uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullNoOpaqueEXT;
  uint flags = gl_RayFlagsOpaqueEXT;
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        pos, tMin, rayDirection, tMax);

  while(rayQueryProceedEXT(rayQuery))
    ;

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true)==gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  return true;
  }

float calcRayShadow(vec4 pos4, vec3 normal, float depth) {
  vec4 sp = scene.viewShadow[1]*pos4;
  if(all(lessThan(abs(sp.xy),vec2(abs(sp.w)))))
    return 1.0;
  float dist = linearDepth(depth, scene.clipInfo);
  vec3  p    = pos4.xyz/pos4.w + dist*0.1*normal; // bias
  if(!rayTest(p,50,10000))
    return 1.0;
  return 0.0;
  }
#endif

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

  const float light  = lambert(normal);

  float shadow = 1;
  if(light>0) {
    const vec4 wpos = worldPos(fragCoord, d);
    shadow = calcShadow(wpos);
#if defined(RAY_QUERY)
    if(shadow>0)
      shadow *= calcRayShadow(wpos,normal,d);
#endif
    }

  const vec3  lcolor = scene.sunCl.rgb*light*shadow + scene.ambient;

  outColor = vec4(diff.rgb*lcolor, diff.a);

  // outColor = vec4(vec3(lcolor), diff.a);
  }
