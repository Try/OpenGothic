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
#if defined(SURFEL_GI)
layout(binding  = 3) uniform sampler2D  surfGi;
layout(binding  = 4) uniform sampler2D  ssao;
layout(binding  = 5, std430) readonly buffer SB0 { uint count;  } surf;
#elif defined(SSAO)
layout(binding  = 3) uniform texture2D  irradiance;
layout(binding  = 4) uniform sampler2D  ssao;
#else
layout(binding  = 3) uniform texture2D  irradiance;
#endif

layout(location = 0) out vec4 outColor;

vec2 uv = gl_FragCoord.xy*scene.screenResInv;

#if defined(SSAO)
float textureSsao() { return textureLod(ssao, uv, 0).r; }
#else
float textureSsao() { return 1; }
#endif

#if !defined(SURFEL_GI)
vec3 skyIrradiance(vec3 n) {
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
  }
#endif

float grayscale(vec3 color) {
  return dot(color, vec3(0.2125, 0.7154, 0.0721));
  }

vec3 luminance(vec3 norm) {
#if defined(SURFEL_GI)
  vec3 ambient = textureLod(surfGi, uv, 0).rgb;
  return ambient;
#else
  vec3 ambient = scene.ambient;
  vec3 sky     = skyIrradiance(norm);

  vec3 ret  = vec3(0);
  ret += ambient;
  ret += sky*0.8;
  ret += (norm.y*0.25+0.75) * NightAmbient * Fd_Lambert;
  return ret;
#endif
  }

void main() {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  const vec3  diff = texelFetch(gbufDiffuse, fragCoord, 0).rgb;
  const vec3  norm = normalFetch(gbufNormal, fragCoord);

  // const vec3  linear = vec3(1);
  const vec3  linear = textureAlbedo(diff);
  const float ao     = textureSsao();

  vec3 color = linear;
  color *= luminance(norm);
#if defined(SURFEL_GI)
  //NOTE: pre-exposed already
  color *= ao;
#else
  color *= ao;
  color *= scene.exposure;
#endif

#if defined(SURFEL_GI) && 1
  if(drawInt(fragCoord.xy-ivec2(100,100), int(surf.count))>0) {
    outColor = vec4(1);
    return;
    }
#endif

  outColor = vec4(color, 1);
  // outColor = vec4(vec3(ao), 0);
  // outColor = vec4(linear, 0);
  // outColor = vec4(srgbEncode(linear), 0);
  }
