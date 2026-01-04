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

// Purkinje Shift using Matrices by Mynx (https://www.shadertoy.com/view/lXy3DW)
// [jpatry21]  https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html
// [kirk11]    http://graphics.berkeley.edu/papers/Kirk-PBT-2011-08/
// [cao08]     https://www.ncbi.nlm.nih.gov/pmc/articles/PMC2630540/pdf/nihms80286.pdf
// [smits00]   https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=76e02f42973b8d27d078edce568fde21c08814f0
vec3 purkinjeShift(const vec3 light, float intensity) {
  // constant
  const vec3 m = vec3(0.63721, 0.39242, 1.6064); // maximal cone sensitivity
  const vec3 k = vec3(0.2, 0.2, 0.29);           // rod input strength long/medium/short
  const float K = 45.0;   // scaling constant
  const float S = 10.0;   // static saturation
  const float k3 = 0.6;   // surround strength of opponent signal
  const float rw = 0.139; // ratio of response for white light
  const float p = 0.6189; // relative weight of L cones

  // [jpatry21] slide 164
  // LMSR matrix using Smits method [smits00]
  // Mij = Integrate[ Ei(lambda) I(lambda) Rj(lambda) d(lambda) ]
  const mat4x3 M = mat4x3(
        7.69684945, 18.4248204, 2.06809497,
        2.43113687, 18.6979422, 3.01246326,
        0.28911757, 1.40183293, 13.7922962,
        0.46638595, 15.5643680, 10.0599647
        );

  // [jpatry21] slide 165
  // M with gain control factored in
  // note: the result is slightly different, is this correct?
  // g = rsqrt(1 + (0.33 / m) * (q + k * q.w))
  const mat4x3 Mg = mat4x3(
        vec3(0.33 / m.x, 1, 1) * (M[0] + k.x * M[3]),
        vec3(1, 0.33 / m.y, 1) * (M[1] + k.y * M[3]),
        vec3(1, 1, 0.33 / m.z) * (M[2] + k.z * M[3]),
        M[3]
        );

  // [jpatry21] slide 166
  const mat3x3 A = mat3x3(
        -1, -1, 1,
        1, -1, 1,
        0,  1, 0
        );

  // [jpatry21] slide 167
  // o = (K / S) * N * diag(k) * (diag(m)^-1)
  const mat3x3 N = mat3x3(
        -(k3 + rw),     p * k3,         p * S,
        1.0 + k3 * rw, (1.0 - p) * k3, (1.0 - p) * S,
        0, 1, 0
        );
  const mat3x3 diag_mi = inverse(mat3x3(m.x, 0, 0, 0, m.y, 0, 0, 0, m.z));
  const mat3x3 diag_k = mat3x3(k.x, 0, 0, 0, k.y, 0, 0, 0, k.z);
  const mat3x3 O =  (K / S) * N * diag_k * diag_mi;

  // [jpatry21] slide 168
  // c = M^-1 * A^-1 * o
  const mat3 Mi = inverse(mat3(M));
  const mat3x3 C = transpose(Mi) * inverse(A) * O;

  // map to some kind of mesopic range, this value is arbitrary, use your best approx
  const float scale = 1000.0;

  // reference version
  //vec4 lmsr = (light * scale) * M;
  //vec3 lmsGain = inversesqrt(1.0f + (0.33f / m) * (lmsr.rgb + k * lmsr.w));

  // matrix folded version, ever so slightly different but good enough
  vec4 lmsr = (light * scale) * Mg;
  vec3 lmsGain = inversesqrt(1.0f + lmsr.rgb);
  vec3 rgbGain = C * lmsGain * intensity / scale;
  return rgbGain * lmsr.w;
  }

#endif
