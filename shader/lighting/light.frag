#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(early_fragment_tests) in;

layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform Ubo {
  vec3 origin; //lwc
  } push;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D  gbufDiffuse;
layout(binding  = 2) uniform usampler2D gbufNormal;
layout(binding  = 3) uniform sampler2D  depth;

layout(location = 0) in vec4 cenPosition;
layout(location = 1) in vec3 color;

bool isShadow(vec3 rayOrigin, vec3 direction) {
#if defined(RAY_QUERY)
  vec3  rayDirection = normalize(direction);
  float rayDistance  = length(direction)-3.0;
  float tMin         = 30;
  if(rayDistance<=tMin)
    return false;

  uint flags = gl_RayFlagsTerminateOnFirstHitEXT;
#if !defined(RAY_QUERY_AT)
  flags |= gl_RayFlagsCullNoOpaqueEXT;
#endif

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, RtShadowMask,
                        rayOrigin, tMin, rayDirection, rayDistance);
  rayQueryProceedShadow(rayQuery);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  return true;
#else
  return false;
#endif
  }

void main(void) {
  vec2  scr = (gl_FragCoord.xy/vec2(textureSize(depth,0)))*2.0-1.0;
  float z   = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x; //lwc?

  vec4 pos = scene.viewProjectLwcInv*vec4(scr.x,scr.y,z,1.0);
  pos.xyz/=pos.w;
  pos.xyz += push.origin;

  vec3 ldir = (pos.xyz-cenPosition.xyz);
  //float qDist = dot(ldir,ldir)/(cenPosition.w*cenPosition.w);

  const float distanceSquare = dot(ldir,ldir);
  const float factor         = distanceSquare / (cenPosition.w*cenPosition.w);
  const float smoothFactor   = max(1.0 - factor * factor, 0.0);

  if(factor>1.0)
    discard;

  const vec3 normal = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));
  //float light   = (1.0-qDist)*lambert;

  float lambert = max(0.0,-dot(normalize(ldir),normal));
  float light   = (lambert/max(factor, 0.05)) * (smoothFactor*smoothFactor);
  if(light<=0.0)
    discard;

  pos.xyz = pos.xyz+5.0*normal; //bias
  ldir    = (pos.xyz-cenPosition.xyz);
  if(isShadow(cenPosition.xyz,ldir))
    discard;

  //outColor     = vec4(0.5,0.5,0.5,1);
  //outColor     = vec4(light,light,light,0.0);
  //outColor     = vec4(d.rgb*color*vec3(light),0.0);

  const vec3 d      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).xyz;
  const vec3 linear = textureLinear(d.rgb);

  vec3 color = linear*color*light*Fd_Lambert*0.1;
  //color *= scene.exposure;

  outColor = vec4(color,0.0);
  }
