#version 450

#extension GL_GOOGLE_include_directive    : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1)         uniform texture2D   gbufDiffuse;
layout(binding = 2)         uniform utexture2D  gbufNormal;
layout(binding = 3)         uniform texture2D   depth;
layout(binding = 4)         uniform texture2D   shadowmask;
layout(binding = 5)         uniform texture2D   directLight;

layout(location = 0) out vec4 outColor;

float lambert(vec3 normal) {
  return max(0.0, dot(scene.sunDir,normal));
  }

void main() {
  outColor = vec4(0,0,0, 1);

  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  const vec4  diff        = texelFetch(gbufDiffuse, fragCoord, 0);
  const float shadow      = texelFetch(shadowmask,  fragCoord, 0).r;
  const vec3  localLights = texelFetch(directLight, fragCoord, 0).rgb;
  const float light       = 1;

  const vec3  illuminance = scene.sunColor * light * shadow;
  const vec3  linear      = textureAlbedo(diff.rgb);
  const vec3  luminance   = linear * Fd_Lambert * illuminance;

  const vec3  local       = linear * localLights;

  outColor = vec4(luminance * scene.exposure + local * max(1.0, scene.exposure), 1);
  }
