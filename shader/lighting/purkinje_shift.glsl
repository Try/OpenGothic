#ifndef PURKINJE_SHIFT_GLSL
#define PURKINJE_SHIFT_GLSL

// https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html#/167/0/1
vec3 purkinjeShift(vec3 rgbLightHdr) {
  const mat4x3 matLmsrFromRgb = mat4x3(
        0.31670331, 0.70299344, 0.08120592,
        0.10129085, 0.72118661, 0.12041039,
        0.01451538, 0.05643031, 0.53416779,
        0.01724063, 0.60147464, 0.40056206);

  const mat3 matRgbFromLmsGain = mat3(
        4.57829597, -4.48749114,  0.31554848,
        -0.63342362,  2.03236026, -0.36183302,
        -0.05749394, -0.09275939,  1.90172089);

  vec4 lmsr    = rgbLightHdr * matLmsrFromRgb;
  vec3 lmsGain = 1.0/sqrt(1.0 + lmsr.xyz);
  return (lmsGain * matRgbFromLmsGain) * lmsr.w;
  }

#endif
