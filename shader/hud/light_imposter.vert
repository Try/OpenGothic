#version 450

#include "scene.glsl"

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 color;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, std430) readonly buffer SsboLighting {
  LightSource lights[];
  };
layout(binding = 2) uniform sampler2D img;

const vec2 v[4] = {
  {-1,-1},
  { 1,-1},
  { 1, 1},
  {-1, 1},
  };

const uint index[6] = {
  0, 3, 1, 3, 2, 1,
  };

void main() {
  const vec2 vert = v[index[gl_VertexIndex]];
  const vec3 pos  = lights[gl_InstanceIndex].pos;

  const vec2 size      = textureSize(img,0).xy;
  const vec2 ndcOffset = vert * size * scene.screenResInv;

  vec4 clipPos = scene.viewProject * vec4(pos, 1.0);
  clipPos.xy += ndcOffset * clipPos.w;

  uv          = vert*0.5 + 0.5;
  color       = lights[gl_InstanceIndex].color;
  gl_Position = clipPos;
  }
