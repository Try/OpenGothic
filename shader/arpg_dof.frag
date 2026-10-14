#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

// gothic-diablo: depth of field of the cinematic dialog shots.
// Sharp around the face in focus, the background (and the shoulder of the listener in front) blurred
// with a disk of up to 'maxCoc' pixels; sharp things are not smeared into the blur around them.

#include "scene.glsl"
#include "common.glsl"

layout(push_constant, std140) uniform PushConstant {
  float focus;    // cm from the camera
  float range;    // cm around the focus that stay sharp
  float maxCoc;   // pixels
  float strength; // 0..1
  } push;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D textureC; // the picture (a copy)
layout(binding = 2) uniform sampler2D textureZ; // depth

layout(location = 0) out vec4 outColor;

float coc(float z) {
  const float d  = linearDepth(z, scene.clipInfo);
  const float dd = max(0.0, abs(d-push.focus)-push.range);
  // in front of the focus it grows faster (the listener's shoulder), behind slowly up to the far background
  const float k  = d<push.focus ? dd/(push.focus*0.6) : dd/(push.focus*1.8);
  return clamp(k, 0.0, 1.0)*push.maxCoc*push.strength;
  }

void main() {
  const ivec2 fc   = ivec2(gl_FragCoord.xy);
  const vec2  size = vec2(textureSize(textureC, 0));
  const vec3  c0   = texelFetch(textureC, fc, 0).rgb;
  const float r0   = coc(texelFetch(textureZ, fc, 0).x);
  if(r0<0.5) {
    outColor = vec4(c0, 1.0);
    return;
    }

  // golden-angle disk
  const int N   = 24;
  vec3      acc = c0;
  float     sum = 1.0;
  [[unroll]]
  for(int i=0; i<N; ++i) {
    const float t   = (float(i)+0.5)/float(N);
    const float r   = sqrt(t)*r0;
    const float a   = float(i)*2.39996323;
    const vec2  off = vec2(cos(a), sin(a))*r;
    const vec2  uv  = (gl_FragCoord.xy+off)/size;
    const vec3  c   = textureLod(textureC, uv, 0).rgb;
    const float rs  = coc(textureLod(textureZ, uv, 0).x);
    // a sample counts when its own blur reaches this far (sharp faces do not bleed into the background)
    const float w   = clamp(rs-r+1.0, 0.0, 1.0);
    acc += c*w;
    sum += w;
    }
  outColor = vec4(acc/sum, 1.0);
  }
