#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky_common.glsl"

layout(binding  = 0) uniform sampler2D tLUT;
layout(binding  = 1) uniform sampler2D mLUT;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

const int numScatteringSteps = 32;

vec3 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax) {
  float cosTheta = dot(rayDir, sunDir);

  float miePhaseValue      = miePhase(cosTheta);
  float rayleighPhaseValue = rayleighPhase(-cosTheta);

  vec3  lum                = vec3(0.0);
  vec3  transmittance      = vec3(1.0);

  for(int i=1; i<=numScatteringSteps; ++i) {
    float t  = (float(i)/numScatteringSteps)*tMax;
    float dt = tMax/numScatteringSteps;

    vec3 newPos = pos + t*rayDir;

    vec3  rayleighScattering;
    vec3  extinction;
    float mieScattering;
    scatteringValues(newPos, rayleighScattering, mieScattering, extinction);

    vec3 sampleTransmittance = exp(-dt*extinction);

    vec3 sunTransmittance = textureLUT(tLUT, newPos, sunDir);
    vec3 psiMS            = textureLUT(mLUT, newPos, sunDir);

    vec3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
    vec3 mieInScattering      = mieScattering*(miePhaseValue*sunTransmittance + psiMS);
    vec3 inScattering         = (rayleighInScattering + mieInScattering);

    // Integrated scattering within path segment.
    vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

    lum += scatteringIntegral*transmittance;

    transmittance *= sampleTransmittance;
    }

  return lum;
  }

void main() {
  const vec2 uv      = inPos*vec2(0.5)+vec2(0.5);
  const vec3 viewPos = vec3(0.0, RPlanet + push.plPosY, 0.0);
  vec3       rayDir  = vec3(0.0);
  vec3       sunDir  = vec3(0.0);

#if defined(FOG)
  {
    // TODO: optimize
    vec3  pos1 = inverse(vec3(inPos,1));
    vec3  pos0 = inverse(vec3(inPos,0));
    rayDir = normalize(pos1-pos0);
    sunDir = vec3(push.sunDir);
  }
#else
  {
    float azimuthAngle = (uv.x - 0.5)*2.0*PI;
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
    const float horizonAngle  = safeacos(sqrt(height*height - RPlanet*RPlanet) / height) - 0.5*PI;
    const float altitudeAngle = adjV*0.5*PI - horizonAngle;

    float cosAltitude = cos(altitudeAngle);

    float sunAltitude = (0.5*PI) - acos(dot(push.sunDir, up));

    sunDir = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
    rayDir = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));
  }
#endif

#if defined(FOG)
  float tMax       = fogFarDistance;
#else
  float atmoDist   = rayIntersect(viewPos, rayDir, RAtmos);
  float groundDist = rayIntersect(viewPos, rayDir, RPlanet);
  float tMax       = (groundDist < 0.0) ? atmoDist : groundDist;
#endif
  vec3  sun        = raymarchScattering(viewPos, rayDir, sunDir, tMax);
  vec3  moon       = raymarchScattering(viewPos, rayDir, normalize(vec3(0,4,1)), tMax)*0.005;

  outColor = vec4(sun+moon, 1.0);
  }
