#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "scene.glsl"

struct Probe {
  vec3 pos;
  uint tid;
  vec3 color;
  };

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1, std430) readonly buffer Pbo { uint probeCount; Probe probe[]; };

layout(location = 0) out vec3 color;
layout(location = 1) out vec3 center;

const vec3 v[8] = {
  {-1,-1,-1},
  { 1,-1,-1},
  { 1, 1,-1},
  {-1, 1,-1},

  {-1,-1, 1},
  { 1,-1, 1},
  { 1, 1, 1},
  {-1, 1, 1},
  };

const uint index[36] = {
  0, 1, 3, 3, 1, 2,
  1, 5, 2, 2, 5, 6,
  5, 4, 6, 6, 4, 7,
  4, 0, 7, 7, 0, 3,
  3, 2, 7, 7, 2, 6,
  4, 5, 0, 0, 5, 1
  };

void main() {
  if(gl_InstanceIndex>=probeCount) {
    gl_Position = vec4(0);
    return;
    }

  Probe p = probe[gl_InstanceIndex];
  //p.pos = vec3(0);

  float range = 5;
  vec3  vert  = v[index[gl_VertexIndex]];

  gl_Position = scene.viewProject * vec4(p.pos + vert * range, 1.0);
  color       = p.color;
  center      = p.pos;
  }
