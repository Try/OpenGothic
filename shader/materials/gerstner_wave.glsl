#ifndef GERSTNER_WAVE_GLSL
#define GERSTNER_WAVE_GLSL

#include "../common.glsl"

struct Wave {
  vec3 offset;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  };

bool gerstnerWave(inout Wave w, vec2 dir, float amplitude, float wavelength, vec3 pos, float speed,
                  float dT, float minLength) {
  const float time = (scene.tickCount32*0.001 + dT)*speed;
  //const float wavelength = 2*M_PI * amplitude / steepness;

  float steepness = clamp(2*M_PI * amplitude / wavelength, 0.1, 0.9);
  //wavelength = max(wavelength, 2*M_PI * amplitude / steepness);
  if(wavelength<minLength)
    ;//return false;

  float mul = smoothstep(minLength, minLength+100, wavelength);
  steepness *= mul;

  float k   = 2 * M_PI / wavelength;
  float c   = sqrt(9.8 / k);
  vec2  d   = normalize(dir.xy);
  float f   = k * (dot(d, pos.xz) - c * time);
  float a   = (steepness / k);

  w.tangent += vec3(
        -d.x * d.x * (steepness * sin(f)),
        +d.x *       (steepness * cos(f)),
        -d.x * d.y * (steepness * sin(f))
        );
  w.binormal += vec3(
        -d.x * d.y * (steepness * sin(f)),
               d.y * (steepness * cos(f)),
        -d.y * d.y * (steepness * sin(f))
        );
  w.offset += vec3(d.x * (a * cos(f)),
                         (a * sin(f)),
                   d.y * (a * cos(f)));
  return true;
  }

// TODO
vec2 dir(float a) {
  float ax = (M_PI*a/180.0);
  return vec2(cos(ax), sin(ax));
  }

Wave gerstnerWave(vec3 p, float minLength) {
  Wave w;
  w.offset   = vec3(0);
  w.normal   = vec3(0);
  w.tangent  = vec3(1,0,0);
  w.binormal = vec3(0,0,1);

#if defined(WATER)
  float amplitude = max(0.1, bucket.waveMaxAmplitude*0.5);
#else
  float amplitude = 0.1;
#endif

  if(true){
#if 1
    // main (ocean) waves
    gerstnerWave(w, vec2(1.00,     0), amplitude*1.00,  600, p,  6.0, 0.5, minLength);
    gerstnerWave(w, vec2(0.99,  0.14), amplitude*0.55,  700, p, 11.0, 0.1, minLength);
    gerstnerWave(w, vec2(0.99, -0.14), amplitude*0.45, 1100, p, 14.0, 0.9, minLength);
#endif

#if 1 && defined(FRAGMENT)
    // secondary wave
    gerstnerWave(w, dir(55+ 0), amplitude*0.15, 75.7, p, 1.160, 0.3, minLength);
    gerstnerWave(w, dir(55+12), amplitude*0.14, 50.3, p, 0.710, 0.2, minLength);
    gerstnerWave(w, dir(55+24), amplitude*0.14, 37.0, p, 0.730, 0.8, minLength);

    // gerstnerWave(w, dir(55+  0), amplitude*0.05, 257, p, 1.160, 0.3, minLength);
    // gerstnerWave(w, dir(55+120), amplitude*0.04, 151, p,  .710, 0.2, minLength);
    // gerstnerWave(w, dir(55+240), amplitude*0.04, 193, p,  .730, 0.8, minLength);
#endif

  }

  w.normal = normalize(cross(w.binormal,w.tangent));
  return w;
  }

#endif
