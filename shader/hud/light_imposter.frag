#version 460

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec2 uv;
layout(location = 1) in  vec3 color;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2) uniform sampler2D img;


void main() {
  vec4 cl = texture(img, uv);
  if(cl.a<0.1)
    discard;
  cl.rgb *= /*srgbDecode*/(color);
  outColor = cl;
  }
