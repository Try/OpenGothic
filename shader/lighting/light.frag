#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#if defined(VIRTUAL_SHADOW)
#include "virtual_shadow/vsm_common.glsl"
#endif

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(early_fragment_tests) in;

layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform Pbo {
  vec3  origin; //lwc
  } push;
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D  gbufDiffuse;
layout(binding  = 2) uniform usampler2D gbufNormal;
layout(binding  = 3) uniform sampler2D  depth;

#if defined(VIRTUAL_SHADOW)
layout(binding  = 5, std430) readonly buffer Omni  { uint pageTblOmni[]; };
layout(binding  = 6)         uniform texture2D pageData;
#endif

layout(location = 0) in vec4      cenPosition;
layout(location = 1) in vec3      color;
layout(location = 2) in flat uint lightId;

bool isShadow(vec3 rayOrigin, vec3 direction, float R) {
#if defined(RAY_QUERY)
  {
    const float dirLength    = length(direction);
    const vec3  rayDirection = -direction/dirLength;
    const float rayDistance  = dirLength - 30; //NOTE: padding of 30cm, in case if light inside wall
    if(rayDistance<=0)
      return false;

    uint flags = RayFlagsShadow;
#if !defined(RAY_QUERY_AT)
    flags |= gl_RayFlagsOpaqueEXT;
#endif

    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, flags, CM_ShadowCaster,
                          rayOrigin, 0.0, rayDirection, rayDistance);
    rayQueryProceedShadow(rayQuery);
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
      return false;
    return true;
  }
#elif defined(VIRTUAL_SHADOW)
  {
    const uint face   = vsmLightDirToFace(direction);
    //if(face!=0 && face!=3)
    //  return false;

    const uint pageD  = pageTblOmni[lightId*6 + face];
    const uint pageId = pageD >> 16u;
    const uint pageSz = pageD & 0xF;

    if(pageSz==0)
      return false;

    const vec3  dir         = vsmMapDirToFace(direction, face);
    const vec2  tc          = (dir.xy/dir.z)*0.5 + 0.5;
    const ivec2 at          = ivec2(tc*pageSz*VSM_PAGE_SIZE);
    const ivec2 pageImageAt = unpackVsmPageId(pageId)*VSM_PAGE_SIZE + at;
    const float z           = texelFetch(pageData, pageImageAt, 0).x;

    const float refZ = vsmApplyProjective(dir.z/R);
    if(z < refZ)
      return false;
    return true;
  }
#else
  return false;
#endif
  }

void main() {
  vec2  scr = (gl_FragCoord.xy/vec2(textureSize(depth,0)))*2.0-1.0;
  float z   = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;

  vec4 pos = scene.viewProjectLwcInv*vec4(scr.x,scr.y,z,1.0);
  pos.xyz/=pos.w;
  pos.xyz += push.origin;

  vec3 ldir = (pos.xyz-cenPosition.xyz);

  const float distanceSquare = dot(ldir,ldir);
  const float factor         = distanceSquare / (cenPosition.w*cenPosition.w);
  const float smoothFactor   = max(1.0 - factor * factor, 0.0);

  if(factor>1.0)
    discard;

  const vec3 normal = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));

  float lambert = max(0.0,-dot(normalize(ldir),normal));
  float light   = (lambert/max(factor, 0.05)) * (smoothFactor*smoothFactor);
  if(light<=0.0)
    discard;

  const float NormalBias = 2.0;
  pos.xyz = pos.xyz + NormalBias*normal; //bias
  if(isShadow(pos.xyz, pos.xyz-cenPosition.xyz, cenPosition.w))
    discard;

  //outColor     = vec4(0.5,0.5,0.5,1);
  //outColor     = vec4(light,light,light,0.0);
  //outColor     = vec4(d.rgb*color*vec3(light),0.0);

  const vec3 d      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).xyz;
  const vec3 linear = textureAlbedo(d.rgb);

  vec3 color = linear*color*light*Fd_Lambert*0.1;
  //color *= scene.exposure;

  outColor = vec4(color,0.0);
  }
