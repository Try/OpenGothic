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
vec4 waterScatter(vec3 back, vec3 normal, float depth) {
  /**
    TODO: Cheap and Convincing Subsurface Scattering Look
    https://www.slideshare.net/colinbb/colin-barrebrisebois-gdc-2011-approximating-translucency-for-a-fast-cheap-and-convincing-subsurfacescattering-look-7170855
    */
  const float attenuation   = min(1.0 - exp(-4.0 * depth), 1.0);
  const float transmittance = exp(-depth*1.5);

  const float lamb = max(dot(scene.sunDir,normal), 0.0);

  // vec3 scatterBase = vec3(0.25,0.55,0.5);
  // vec3 scatterBase = vec3(0.31,0.69,0.76);
  vec3 scatterBase = vec3(0.25,0.55,0.5)/vec3(0.94, 0.87, 0.76);

  // NOTE: need to fix out HDR(overall) and use scene.GSunIntensity
  vec3 scatter = scatterBase * lamb * scene.sunCl.rgb;
  //scatter = scatter * transmittance;

  return vec4(scatter, attenuation);
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

  if(depth==1.0)
    discard;

  float len   = length(wPos-camPos) * 0.0001;
  vec4  color = waterScatter(vec3(1),vec3(0,1,0),len);
  outColor = vec4(color.rgb*color.a, color.a);
  }
