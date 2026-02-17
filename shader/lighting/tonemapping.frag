#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#include "scene.glsl"
#include "common.glsl"
#include "lighting/tonemapping.glsl"
#include "lighting/purkinje_shift.glsl"

#include "upscale/lanczos.glsl"

layout(push_constant, std140) uniform PushConstant {
  VideoSettings settings;
  } push;

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D textureD;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

const uvec3 TONEMAP_DITHER_TARGET_BITS =  uvec3(8, 8, 8); // hardcoded for now -> adjust in future for HDR support or 6 bit monitors

// https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
// https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.40.9608&rep=rep1&type=pdf
vec4 rgb2lmsr(vec3 c) {
  const mat4x3 m = mat4x3(
        0.31670331, 0.70299344, 0.08120592,
        0.10129085, 0.72118661, 0.12041039,
        0.01451538, 0.05643031, 0.53416779,
        0.01724063, 0.60147464, 0.40056206);
  return c * m;
  }

vec3 lms2rgb(vec3 c) {
  const mat3 m = mat3(
        4.57829597, -4.48749114,  0.31554848,
        -0.63342362,  2.03236026, -0.36183302,
        -0.05749394, -0.09275939,  1.90172089);
  return c * m;
  }

// https://www.ncbi.nlm.nih.gov/pmc/articles/PMC2630540/pdf/nihms80286.pdf
vec3 _purkinjeShift(vec3 c) {
  const vec3  m  = vec3(0.63721, 0.39242, 1.6064);
  const float K  = 45.0;
  const float S  = 10.0;
  const float k3 = 0.6;
  const float k5 = 0.2;
  const float k6 = 0.29;
  const float rw = 0.139;
  const float p  = 0.6189;

  vec4 lmsr = rgb2lmsr(c);

  vec3 g = vec3(1.0) / sqrt(vec3(1.0) + (vec3(0.33) / m) * (lmsr.xyz + vec3(k5, k5, k6) * lmsr.w));

  float rc_gr = (K / S) * ((1.0 + rw * k3) * g.y / m.y - (k3 + rw) * g.x / m.x) * k5 * lmsr.w;
  float rc_by = (K / S) * (k6 * g.z / m.z - k3 * (p * k5 * g.x / m.x + (1.0 - p) * k5 * g.y / m.y)) * lmsr.w;
  float rc_lm = K * (p * g.x / m.x + (1.0 - p) * g.y / m.y) * k5 * lmsr.w;

  vec3 lms_gain = vec3(-0.5 * rc_gr + 0.5 * rc_lm, 0.5 * rc_gr + 0.5 * rc_lm, rc_by + rc_lm);
  vec3 rgb_gain = lms2rgb(lms_gain);

  return rgb_gain;
  }

float luminance(vec3 rgb) {
  const vec3 W = vec3(0.2125, 0.7154, 0.0721);
  return dot(rgb, W);
  }

void main() {
  float exposure   = scene.exposure;

#if defined(UPSCALE)
  vec3  color      = lanczosUpscale(textureD, uv).rgb;
#else
  vec3  color      = textureLod(textureD, uv, 0).rgb;
#endif

  {
    // outColor = vec4(srgbEncode(color), 1);
    // outColor = vec4(color, 1);
    // return;
  }

  {
    // outColor = vec4(vec3(luminance(color/exposure)/100000.0), 1);
    // return;
  }

  {
    // night shift
    // const vec3 shift = purkinjeShift(color/exposure)*exposure;
    const vec3 shift = purkinjeShift(color/exposure, 1.5)*exposure;
    //color += shift;
    // in vanilla game night time attenuated as rgb{18 16 60}
    // color += vec3(shift.r, 0, shift.b);
  }

  color    = gameTonemap(color, push.settings);
  color   += dither(gl_FragCoord.xy, TONEMAP_DITHER_TARGET_BITS); 
  outColor = vec4(color, 1.0);
  }
