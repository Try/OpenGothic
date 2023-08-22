#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "scene.glsl"
#include "common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(location = 0) in  vec3 color;
layout(location = 1) in  vec3 center;

layout(location = 0) out vec4 outColor;

vec3 unprojectDepth(float z) {
  const vec2 fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4 pos       = vec4(fragCoord.xy, z, 1.0);///gl_FragCoord.w);
  const vec4 ret       = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

void main(void) {
  vec3 pos1 = unprojectDepth(gl_FragCoord.z);
  vec3 pos0 = unprojectDepth(0);
  vec3 view = normalize(pos1-pos0);

  float t = rayIntersect(pos0-center, view, 5);
  if(t<0) {
    outColor = vec4(1,0,0,0);
    discard;
    }

  vec3  norm  = (pos0+view*t) - center;
  float lamb  = max(dot(scene.sunDir, normalize(norm)), 0);
  float light = lamb*0.7+0.3;

  outColor = vec4(color*light,1.0);
  // outColor = vec4(1,0,0,1.0);
  }
