#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "lighting/tonemapping.glsl"
#include "lighting/purkinje_shift.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D  gbufDiffuse;
layout(binding  = 2) uniform usampler2D gbufNormal;
layout(binding  = 3) uniform sampler2D  irradiance;
#if defined(SSAO)
layout(binding  = 4) uniform sampler2D ssao;
#endif

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

#if defined(SSAO)
float textureSsao() { return textureLod(ssao, uv, 0).r; }
#else
float textureSsao() { return 0; }
#endif

vec3 skyIrradiance() {
#if 0
  return scene.ambient;
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
  const vec3  linear = textureLinear(diff); //  * Fd_Lambert is accounted in integration
  const float ao     = textureSsao();

  vec3 ambient = scene.ambient;
  vec3 sky     = skyIrradiance();

  vec3 lcolor  = mix(ambient, sky, max(0, 0.25 + norm.y*0.25));
  // vec3 lcolor  = ambient + sky*clamp(norm.y*0.5, 0,1);

  vec3 color = lcolor.rgb;
  color *= linear;
  color *= (1-ao);

  // outColor = vec4(vec3(grayscale(color)), 0);
  // return;

  // night shift
  color += purkinjeShift(color); //TODO: use it globally at tonemapping
  color *= scene.exposure;

  outColor = vec4(color, 1);
  // outColor = vec4(vec3(1-ao), 0);
  }
