#version 460
#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "lighting/tonemapping.glsl"
#include "lighting/purkinje_shift.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform texture2D  gbufDiffuse;
layout(binding  = 2) uniform usampler2D gbufNormal;
layout(binding  = 3) uniform texture2D  irradiance;
#if defined(SSAO)
layout(binding  = 4) uniform sampler2D ssao;
#endif

layout(location = 0) out vec4 outColor;

vec2 uv = gl_FragCoord.xy*scene.screenResInv;

#if defined(SSAO)
float textureSsao() { return textureLod(ssao, uv, 0).r; }
#else
float textureSsao() { return 1; }
#endif

vec3 skyIrradiance() {
#if 0
  return scene.ambient * Fd_LambertInv;
#else
  vec3 n = texelFetch(gbufNormal, ivec2(gl_FragCoord.xy), 0).rgb;
  n = normalize(n*2.0 - vec3(1.0));

  ivec3 d;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = n*n;

  vec3 ret = vec3(0);
  ret += texelFetch(irradiance, ivec2(0,d.x), 0).rgb * n.x;
  ret += texelFetch(irradiance, ivec2(1,d.y), 0).rgb * n.y;
  ret += texelFetch(irradiance, ivec2(2,d.z), 0).rgb * n.z;

  return ret;
#endif
  }

float grayscale(vec3 color) {
  return dot(color, vec3(0.2125, 0.7154, 0.0721));
  }

void main() {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  const vec3  diff = texelFetch(gbufDiffuse, fragCoord, 0).rgb;
  const vec3  norm = normalFetch(gbufNormal, fragCoord);

  // const vec3  linear = vec3(1);
  const vec3  linear = textureAlbedo(diff);
  const float ao     = textureSsao();

  vec3 ambient = scene.ambient + (norm.y*0.25+0.75) * NightAmbient * Fd_Lambert;
  vec3 sky     = skyIrradiance(); // * Fd_Lambert is accounted in integration

  vec3 luminance  = sky + ambient;

  vec3 color = luminance;
  color *= linear;
  color *= ao;
  color *= scene.exposure;

  outColor = vec4(color, 1);
  // outColor = vec4(vec3(ao), 0);
  // outColor = vec4(linear, 0);
  // outColor = vec4(srgbEncode(linear), 0);
  }
