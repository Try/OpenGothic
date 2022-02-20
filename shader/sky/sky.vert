#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#define VERTEX
#include "sky_common.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in  vec2 inPos;

layout(location = 0) out vec2 outPos;
layout(location = 1) out vec3 skyColor;

void main() {
#if defined(FOG)
  vec3  pos   = vec3(0,RPlanet+ubo.plPosY,0);
  skyColor    = atmosphere(pos,vec3(0,1,0),ubo.sunDir);
#else
  skyColor    = vec3(1,0,0);
#endif
  outPos      = inPos;
  gl_Position = vec4(inPos.xy, 1.0, 1.0);
  }
