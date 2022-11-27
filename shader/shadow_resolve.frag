#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "scene.glsl"
#include "lighting/shadow_sampling.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(binding = 1) uniform sampler2D diffuse;
layout(binding = 2) uniform sampler2D normals;
layout(binding = 3) uniform sampler2D depth;

layout(binding = 4) uniform sampler2D textureSm0;
layout(binding = 5) uniform sampler2D textureSm1;

float calcShadow(ivec2 frag, float depth) {
  const vec2 fragCoord = (frag.xy*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec4 pos4      = scene.viewProjectInv * scr;

  return calcShadow(pos4, scene, textureSm0, textureSm1);
  }

float lambert(vec3  normal) {
  return max(0.0, dot(scene.sunDir,normal));
  }

void main(void) {
  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const float d         = texelFetch(depth, fragCoord, 0).r;
  if(d==1.0) {
    outColor = vec4(0);
    return;
    }

  const vec4  diff   = texelFetch(diffuse, fragCoord, 0);
  const vec4  nrm    = texelFetch(normals, fragCoord, 0);
  const vec3  normal = normalize(nrm.xyz*2.0-vec3(1.0));

  const float shadow = calcShadow(fragCoord, d);
  const float light  = lambert(normal)*shadow;
  const vec3  lcolor = scene.sunCl.rgb*light + scene.ambient;

  outColor = vec4(diff.rgb*lcolor, diff.a);

  // outColor = vec4(vec3(lcolor), diff.a);
  }
