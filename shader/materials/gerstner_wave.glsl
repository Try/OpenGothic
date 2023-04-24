#ifndef GERSTNER_WAVE_GLSL
#define GERSTNER_WAVE_GLSL

#include "../common.glsl"

struct Wave {
  vec3 offset;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  };

vec2 gerWave(inout Wave w, vec2 d, float amplitude, vec2 pos, float speed, float frequency) {
  float x = dot(d, pos) * frequency + scene.tickCount32*0.001*speed;
  float a = amplitude;

  w.binormal += vec3(
        -d.x * d.y * (a * sin(x)),
               d.y * (a * cos(x)),
        -d.y * d.y * (a * sin(x))
        ) * frequency;
  w.tangent += vec3(
        -d.x * d.x * (a * sin(x)),
        +d.x *       (a * cos(x)),
        -d.x * d.y * (a * sin(x))
        ) * frequency;
  w.offset += vec3(d.x * (a * cos(x)),
                         (a * sin(x)),
                   d.y * (a * cos(x)));

  return vec2(a * sin(x), a);
  }

vec2 expWave(inout Wave w, vec2 dir, float amplitude, vec2 pos, float speed, float frequency) {
  // Based on: https://www.shadertoy.com/view/MdXyzX
  float x    = dot(dir, pos) * frequency + scene.tickCount32*0.001*speed;

  float wave = 2.0 * (exp(sin(x) - 1.0)) * amplitude;
  float dx   = wave * cos(x);

  w.offset.y += wave;
  w.tangent  += vec3(-dir.y, dir.x,      0) * dx * frequency;
  w.binormal += vec3(     0, dir.y, -dir.y) * dx * frequency;

  return vec2(wave, -dx);
  }

// returns vec2 with wave height in X and its derivative in Y
vec2 wave(inout Wave w, vec2 dir, float amplitude, vec2 pos, float speed, float frequency) {
#if 1
  return gerWave(w,dir,amplitude,pos,speed,frequency);
#else
  return expWave(w,dir,amplitude,pos,speed,frequency);
#endif
  }

Wave wave(vec3 pos, float minLength) {
  const float amplitudeMin = 10;
  const float dragMult     = 0.048;

#if defined(FRAGMENT)
  const int iterations = 43;
#else
  const int iterations = 13;
#endif

#if defined(WATER)
  const float amplitude = max(amplitudeMin, bucket.waveMaxAmplitude*0.5);
#else
  const float amplitude = amplitudeMin;
#endif

  float wx   = 1.0;
  float wsum = 0.0;
  for(int i=0; i<iterations; i++) {
    wsum += wx;
    wx   *= 0.8;
    }

  Wave  w;
  w.offset   = vec3(0);
  w.normal   = vec3(0,1,0);
  w.tangent  = vec3(1,0,0);
  w.binormal = vec3(0,0,1);

  float freq   = 0.6 * 0.005;
  float speed  = 2.0;
  float iter   = 0.0;
  float weight = 1.0;

  for(int i=0; i<iterations; i++) {
    if(freq*minLength > 2.0)
      continue;

    vec2 dir = vec2(cos(iter), sin(iter));
    vec2 res = wave(w, dir, weight*amplitude/wsum, pos.xz, speed, freq);

    pos.xz += res.y * weight * dir * dragMult;

    iter   += 12.0;
    weight *= 0.8;
    freq   *= 1.18;
    speed  *= 1.07;
    }

#if defined(FRAGMENT)
  w.normal = normalize(cross(w.binormal,w.tangent));
#endif

  return w;
  }

#endif
