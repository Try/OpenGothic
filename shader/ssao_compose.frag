#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "common.glsl"

layout(push_constant, std140) uniform PushConstant {
  mat4 mvpInv;
  vec3 ambient;
  vec3 ldir;
  } ubo;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D ssao;
layout(binding  = 3) uniform sampler2D depth;

#if defined(RAY_QUERY)
layout(binding  = 5) uniform accelerationStructureEXT topLevelAS;
#endif

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 inverse(vec3 pos) {
  vec4 ret = ubo.mvpInv*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

#if defined(RAY_QUERY)
bool rayTest(vec3 pos0, vec3 dp) {
  float tMin         = 150;
  float rayDistance  = 100 * 100; // 100 meters
  vec3  rayDirection = /*normalize*/(dp);

  //uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullNoOpaqueEXT;
  uint flags = gl_RayFlagsOpaqueEXT;
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        pos0, tMin, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery))
    ;

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true)==gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  return true;
  }
#endif
/*
bool calcShadow(vec3 shPos0, vec3 shPos1) {

  if(abs(shPos0.x)<1.0 && abs(shPos0.y)<1.0 && ((0.45<lay1.x && lay1.x<0.55) || lay1.x==0))
    return v0;
  if(abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0)
    return v1;
  return 1.0;
  }*/

void main() {
  vec4  lbuf    = textureLod(lightingBuf,uv,0);
  vec3  clr     = srgbDecode(textureLod(diffuse,    uv,0).rgb);
  float occ     = textureLod(ssao,       uv,0).r;
  vec3  ambient = ubo.ambient;

#if defined(RAY_QUERY)
  {
    vec4 z   = textureLod(depth,uv,0);
    vec2 scr = uv*2.0-vec2(1.0);

    vec3 pos = inverse(vec3(scr.x,scr.y,z.x));
    // TODO: test only, if shadowmap toesnt take care
    if(rayTest(pos,ubo.ldir)) {
      lbuf.rgb = clr*ambient;
      }
  }
#endif
  // outColor = vec4(1-occ);
  outColor = vec4(lbuf.rgb-clr*ambient*occ, lbuf.a);
  }
