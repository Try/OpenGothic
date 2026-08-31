#version 450

#include "scene.glsl"

layout(location = 0) out flat uint  axis;
layout(location = 1) out flat float scale;

layout(std140, push_constant) uniform Push {
  vec3 origin;
  };

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

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
  const vec3 vert = v[index[gl_VertexIndex]];

  scale = (scene.viewProject * vec4(origin, 1.0)).w/1000.0;

  vec3  size  = vec3(200,10,10) * scale;
  vec3  pos   = vert * size;
  pos.x = max(pos.x, 10 * scale);

  if(gl_InstanceIndex==0) {
    pos  = pos.xyz;
    axis = 0;
    }
  else if(gl_InstanceIndex==1) {
    pos  = pos.yxz;
    axis = 1;
    }
  else if(gl_InstanceIndex==2) {
    pos = pos.yzx;
    axis = 2;
    }
  else if(gl_InstanceIndex==3) {
    pos = pos.zzz;
    axis = 4;
    }
  gl_Position = scene.viewProject * vec4(origin + pos, 1.0);
  }
