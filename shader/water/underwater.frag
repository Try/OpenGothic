#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#if defined(RAY_QUERY_AT)
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_flags_primitive_culling : enable
#endif

#include "../scene.glsl"
#include "../common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D zbuffer;

// fixme: copy-paste
vec4 waterScatter(vec3 back, vec3 normal, const float len) {
  const float depth         = len / 5000.0; // 50 meters
  const vec3  transmittance = exp(-depth * vec3(4,2,1) * 1.25);
#if defined(SCATTERING)
  const float f       = fresnel(scene.sunDir,normal,IorWater);
  const vec3  scatter = f * scene.sunCl.rgb * scene.GSunIntensity * (1-exp(-len/20000.0));
  return vec4(scatter*transmittance, 1);
#else
  return vec4(transmittance, 1);
#endif
  }

vec3 unproject(vec4 screen) {
  const vec4 pos4 = scene.viewProjectInv * screen;
  return pos4.xyz/pos4.w;
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const float depth     = texelFetch(zbuffer,  ivec2(gl_FragCoord.xy), 0).r;

  const vec3  camPos    = unproject(vec4(0,0,0, 1.0));
  const vec3  wPos      = unproject(vec4(fragCoord.x, fragCoord.y, depth, 1.0));

  const float len = length(wPos-camPos);
  outColor = waterScatter(vec3(1),vec3(0,1,0),len);
  }
