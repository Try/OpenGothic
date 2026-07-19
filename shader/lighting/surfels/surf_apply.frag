#version 450

#include "lighting/surfels/surf_common.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3  surfPos;
layout(location = 1) in  float surfRadius;
layout(location = 2) in  vec3  surfNorm;
layout(location = 3) in  vec4  surfIrr;

layout(std140, push_constant) uniform Push {
  vec3 originLwc;
  uint tileSize;
  uint pass;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, rgba16f) uniform image2D irradiance;
layout(binding = 2) uniform utexture2D gbufNormal;
layout(binding = 3) uniform texture2D  depth;
layout(binding = 4, std430) readonly buffer SB0 { SurfHeader header; Surfel surfels[]; };

vec3 worldPos(float z, ivec2 fragCoord, ivec2 screenSize) {
  const mat4 inv = scene.viewProjectLwcInv;
  const vec2 uv  = vec2(fragCoord+0.5)*scene.screenResInv;
  const vec4 pos = inv*vec4(uv * 2.0 - 1.0, z, 1);
  return pos.xyz/pos.w + originLwc;
  }

void main() {
  const ivec2 fragCoord  = ivec2(gl_FragCoord.xy);
  const ivec2 screenSize = ivec2(textureSize(depth,0));

  const float z    = texelFetch(depth, fragCoord, 0).x;
  const vec3  wpos = worldPos(z, fragCoord, screenSize);
  const vec3  norm = normalFetch(gbufNormal, fragCoord);

  const float w     = calculteWeight(surfPos, surfNorm, surfIrr.w, surfRadius, wpos, norm);
  const vec3  color = surfIrr.rgb * scene.exposure;
  if(w <= 0.0)
    discard;

  outColor = vec4(color*w, w);
  }
