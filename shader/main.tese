#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(triangles, fractional_odd_spacing, cw) in;

#define TESSELATION
#include "shader_common.glsl"

layout(location = 0) in  Varyings shInp[];
layout(location = 0) out Varyings shOut;

vec2 interpolate(vec2 v0, vec2 v1, vec2 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

vec3 interpolate(vec3 v0, vec3 v1, vec3 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }


vec4 wave(vec2 pos) {
  vec4 up = vec4(scene.viewProject[1]);
  return sin(pos.x*10.0*M_PI + material.waveAnim) * up * 5;
  }

void main() {
  shOut.uv           = interpolate(shInp[0].uv,           shInp[1].uv,           shInp[2].uv);
#if !defined(SHADOW_MAP)
  shOut.shadowPos[0] = interpolate(shInp[0].shadowPos[0], shInp[1].shadowPos[0], shInp[2].shadowPos[0]);
  shOut.shadowPos[1] = interpolate(shInp[0].shadowPos[1], shInp[1].shadowPos[1], shInp[2].shadowPos[1]);
  shOut.normal       = interpolate(shInp[0].normal,       shInp[1].normal,       shInp[2].normal);
  shOut.pos          = interpolate(shInp[0].pos,          shInp[1].pos,          shInp[2].pos);
#endif
#if defined(VCOLOR) && !defined(SHADOW_MAP)
  shOut.color        = interpolate(shInp[0].color,        shInp[1].color,        shInp[2].color);
#endif

  vec4 pos           = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
  pos += wave(shOut.uv.xy);

  shOut.scr          = pos;

  gl_Position = pos;
  }
