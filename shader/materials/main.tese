#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(triangles, fractional_odd_spacing, cw) in;

#include "materials_common.glsl"
#include "water/gerstner_wave.glsl"

#if defined(BINDLESS) && defined(MAT_VARYINGS)
layout(location = 0) in  flat uint bucketIdIn[];
layout(location = 1) in  Varyings  shInp[];

layout(location = 0) out flat uint bucketId;
layout(location = 1) out Varyings  shOut;
#else
const uint bucketId = 0;
layout(location = 0) in  Varyings shInp[];
layout(location = 0) out Varyings shOut;
#endif

vec2 interpolate(vec2 v0, vec2 v1, vec2 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

vec3 interpolate(vec3 v0, vec3 v1, vec3 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

void main() {
  shOut.uv           = interpolate(shInp[0].uv,           shInp[1].uv,           shInp[2].uv);
#if !defined(DEPTH_ONLY)
  shOut.normal       = interpolate(shInp[0].normal,       shInp[1].normal,       shInp[2].normal);
  shOut.pos          = interpolate(shInp[0].pos,          shInp[1].pos,          shInp[2].pos);
#endif
#if defined(MAT_COLOR)
  shOut.color        = interpolate(shInp[0].color,        shInp[1].color,        shInp[2].color);
#endif

  vec4 pos           = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);

#if defined(BINDLESS)
  bucketId = bucketIdIn[0];
#endif

#if defined(BINDLESS)
    const float waveMaxAmplitude = bucket[bucketId].waveMaxAmplitude;
#else
    const float waveMaxAmplitude = bucket.waveMaxAmplitude;
#endif

  // vec3 dPos = multiWave(shOut.pos.xz*0.001);
  if(waveMaxAmplitude>0) {
    Wave w = wave(shOut.pos, 0.0);
    pos          += scene.viewProject*vec4(w.offset,0);
    shOut.pos    += w.offset;
    shOut.normal  = w.normal;
    } else {
    shOut.normal = normalize(shOut.normal);
    }

  gl_Position = pos;
  }
