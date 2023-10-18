#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky_common.glsl"

layout(binding  = 0) uniform sampler2D tLUT;
layout(binding  = 1) uniform sampler2D mLUT;
layout(binding  = 2) uniform sampler2D cloudsLUT;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

const int numScatteringSteps = 32;

vec3 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax) {
  float cosTheta = dot(rayDir, sunDir);

  float miePhaseValue      = miePhase(cosTheta);
  float rayleighPhaseValue = rayleighPhase(-cosTheta);

  vec3  scatteredLight = vec3(0.0);
  vec3  transmittance  = vec3(1.0);

  const float clouds = textureLod(cloudsLUT, vec2(push.night,0), 0).a;

  for(int i=1; i<=numScatteringSteps; ++i) {
    float t  = (float(i)/numScatteringSteps)*tMax;
    float dt = tMax/numScatteringSteps;

    vec3  newPos = pos + t*rayDir;

    vec3  rayleighScattering;
    vec3  extinction;
    float mieScattering;
    scatteringValues(newPos, clouds, rayleighScattering, mieScattering, extinction);

    vec3 sampleTransmittance = exp(-dt*extinction);

    vec3 sunTransmittance = textureLUT(tLUT, newPos, sunDir);
    vec3 psiMS            = textureLUT(mLUT, newPos, sunDir);

    vec3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
    vec3 mieInScattering      = mieScattering     *(miePhaseValue*sunTransmittance      + psiMS);
    vec3 inScattering         = (rayleighInScattering + mieInScattering);

    // Integrated scattering within path segment.
    vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

    scatteredLight += scatteringIntegral*transmittance;
    transmittance  *= sampleTransmittance;
    }

  return scatteredLight;
  }

void main() {
  const vec2 uv       = inPos*vec2(0.5)+vec2(0.5);
  const vec3 viewPos  = vec3(0.0, RPlanet + push.plPosY, 0.0);

  const float DirectSunLux  = 64000.f;
  const float DirectMoonLux = 0.27f;
  const float NightLight    = 0.36f;
  const float moonInt       = DirectMoonLux/DirectSunLux;

  float azimuthAngle = (uv.x - 0.5)*2.0*M_PI;
  // Non-linear mapping of altitude. See Section 5.3 of the paper.
  float adjV;
  if(uv.y < 0.5) {
    float coord = 1.0 - 2.0*uv.y;
    adjV = -coord*coord;
    } else {
    float coord = uv.y*2.0 - 1.0;
    adjV = coord*coord;
    }

  const float height        = length(viewPos);
  const vec3  up            = viewPos / height;
  const float horizonAngle  = safeacos(sqrt(height*height - RPlanet*RPlanet) / height) - 0.5*M_PI;
  const float altitudeAngle = adjV*0.5*M_PI - horizonAngle;

  float cosAltitude = cos(altitudeAngle);
  float sunAltitude = (0.5*M_PI) - acos(dot(push.sunDir, up));

  vec3  sunDir      = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
  vec3  rayDir      = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));

  float atmoDist    = rayIntersect(viewPos, rayDir, RAtmos);
  float groundDist  = rayIntersect(viewPos, rayDir, RPlanet);
  float tMax        = (groundDist < 0.0) ? atmoDist : groundDist;

  vec3  sun  = raymarchScattering(viewPos, rayDir, sunDir, tMax);
  vec3  moon = raymarchScattering(viewPos, rayDir, vec3(0,1,0), tMax) * push.night * moonInt;
  // vec3  moon = raymarchScattering(viewPos, rayDir, normalize(vec3(-1,1,0)), tMax)*push.night*moonInt;
  outColor = vec4(sun+moon, 1.0);
  }
