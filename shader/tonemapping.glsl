#ifndef TONEMAPPING_GLSL
#define TONEMAPPING_GLSL

vec3 reinhardTonemap(vec3 c){
  // From: https://www.shadertoy.com/view/tdSXzD
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  vec3 tc = c / (c + 1.0);
  return mix(c / (l + 1.0), tc, tc);
  }

vec3 invertReinhardTonemap(vec3 c) {
  // rgb / (1 - lum(rgb))
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  return c / max(1.0 / 32768.0, 1.0 - lum);
  }

#endif
