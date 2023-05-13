#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(vertices = 3) out;

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
    vec4 pos0 = gl_in[0].gl_Position;
    vec4 pos1 = gl_in[1].gl_Position;
    vec4 pos2 = gl_in[2].gl_Position;

    gl_TessLevelInner[0] = tessInnerFactor(pos0,pos1,pos2);

    gl_TessLevelOuter[0] = tessFactor(pos1,pos2);
    gl_TessLevelOuter[1] = tessFactor(pos0,pos2);
    gl_TessLevelOuter[2] = tessFactor(pos0,pos1);
    }

  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  shOut[gl_InvocationID]              = shInp[gl_InvocationID];
  }
