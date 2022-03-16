#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(vertices = 3) out;

#define TESSELATION
#include "materials_common.glsl"

in gl_PerVertex {
  vec4 gl_Position;
  } gl_in[gl_MaxPatchVertices];

layout(location = 0) in  Varyings shInp[];
layout(location = 0) out Varyings shOut[];

const int MAX_INNER = 16;
const int MAX_OUTER = 16;

float tessFactor(vec3 a, vec3 b) {
  float l = length(a-b)+0.5;
  return l*MAX_OUTER+1;
  }

float tessInnerFactor(vec2 a, vec2 b, vec2 c) {
  a -= c;
  b -= c;
  float z = a.x*b.y - b.x*a.y;
  return (abs(z)+0.5)*MAX_INNER;
  }

float tessFactor(in vec4 a, in vec4 b) {
  return tessFactor(a.xyz/a.w, b.xyz/b.w);
  }

float tessInnerFactor(in vec4 a, in vec4 b, in vec4 c) {
  return tessInnerFactor(a.xy/a.w, b.xy/b.w, c.xy/c.w);
  }

void main() {
  if(gl_InvocationID==0) {
    gl_TessLevelInner[0] = tessInnerFactor(shInp[0].scr,shInp[1].scr,shInp[2].scr);

    gl_TessLevelOuter[0] = tessFactor(shInp[1].scr,shInp[2].scr);
    gl_TessLevelOuter[1] = tessFactor(shInp[0].scr,shInp[2].scr);
    gl_TessLevelOuter[2] = tessFactor(shInp[0].scr,shInp[1].scr);
    }

  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  shOut[gl_InvocationID]              = shInp[gl_InvocationID];
  }
