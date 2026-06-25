#version 460

#include "lighting/surfels/surf_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, std430) readonly buffer SB0  { uvec4 count; Surfel surfels[]; };
layout(binding = 2) uniform usampler2D gbufNormal;
layout(binding = 3) uniform sampler2D  depth;

layout(location = 0) in  vec3  center;
layout(location = 1) in  vec3  normal;
layout(location = 2) in  float radius;
layout(location = 3) in  flat uint instanceIndex;

layout(location = 0) out vec4 outColor;

vec3 unprojectDepth(float z) {
  const vec2 fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4 pos       = vec4(fragCoord.xy, z, 1.0);///gl_FragCoord.w);
  const vec4 ret       = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

void main(void) {
  //vec3 norm = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));
  float z    = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;
  vec3  wpos = unprojectDepth(z);
  vec3  view = normalize(wpos-scene.camPos);

  const Surfel p = surfels[instanceIndex];
  const vec3  delta          = wpos - center;
  const float distToPlane    = dot(delta, normal);
  const vec3  projectedPoint = (delta - distToPlane * normal) / radius;
  const float qDist          = dot(projectedPoint, projectedPoint);

  if(qDist > 1)
    discard;

  if(length(delta) > radius)
    ;//discard;

  //vec3 clr = surfDebugColor(p, instanceIndex) * (1.0-qDist);
  //vec3 clr = 3.0 * p.irradiance * (1.0-qDist) * scene.exposure;
  vec3 clr = p.irradiance * (1.0-qDist);
  outColor = vec4(clr,1.0);
  // outColor = vec4(1,0,0,1.0);
  }
