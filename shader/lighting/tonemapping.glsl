#ifndef TONEMAPPING_GLSL
#define TONEMAPPING_GLSL

#include "common.glsl"

vec3 jodieReinhardTonemapInv(vec3 c) {
  // rgb / (1 - lum(rgb))
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  return c / max(1.0 / 32768.0, 1.0 - lum);
  }

vec3 jodieReinhardTonemap(vec3 c){
  // From: https://www.shadertoy.com/view/tdSXzD
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  vec3 tc = c / (c + 1.0);
  return mix(c / (l + 1.0), tc, tc);
  }

vec3 acesTonemap(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
  }

vec3 acesTonemapInv(vec3 x) {
  // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return (-0.59 * x + 0.03 - sqrt(-1.0127 * x*x + 1.3702 * x + 0.0009)) / (2.0 * (2.43*x - 2.51));
  }

// HACK: need to preserve look-and-fill of original graphics
vec3 textureLinear(vec3 rgb) {
  const vec3 linear = srgbDecode(rgb);
#if defined(EMISSIVE)
  // emissive objects, spells
  return acesTonemapInv(linear);
#else
  // return vec3(0.52, 0.41, 0.36); // wood
  // return vec3(0.33, 0.34, 0.18); // leaves
  // return vec3(0.9);
  // return acesTonemapInv(linear*0.8);
  return acesTonemapInv(linear*0.78+0.001)*Fd_LambertInv; // adjusted to have 'realistic' albedo values
#endif
  }

#endif
