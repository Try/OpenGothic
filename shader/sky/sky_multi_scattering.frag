#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky_common.glsl"

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

layout(binding  = 0) uniform sampler2D tLUT;

// Each pixel coordinate corresponds to a height and sun zenith angle, and
// the value is the multiple scattering approximation (Psi_ms from the paper, Eq. 10).
const int mulScattSteps = 20;
const int sqrtSamples   = 8;

vec3 sphericalDir(float theta, float phi) {
  float c = cos(phi);
  float s = sin(phi);
  return vec3(s*sin(theta), c, s*cos(theta));
  }

// Calculates Equation (5) and (7) from the paper.
void mulScattValues(vec3 pos, vec3 sunDir, out vec3 lumTotal, out vec3 fms) {
  lumTotal = vec3(0.0);
  fms      = vec3(0.0);

  float invSamples = 1.0/float(sqrtSamples*sqrtSamples);
  for(int i=0; i<sqrtSamples; i++) {
    for(int j=0; j<sqrtSamples; j++) {
      // This integral is symmetric about theta = 0 (or theta = PI), so we
      // only need to integrate from zero to PI, not zero to 2*PI.
      float theta  = M_PI * (float(i) + 0.5) / float(sqrtSamples);
      float phi    = safeacos(1.0 - 2.0*(float(j) + 0.5) / float(sqrtSamples));
      vec3  rayDir = sphericalDir(theta, phi);

      float atmoDist   = rayIntersect(pos, rayDir, RAtmos);
      float groundDist = rayIntersect(pos, rayDir, RPlanet);
      float tMax = atmoDist;
      if(groundDist>0.0)
        tMax = groundDist;

      float cosTheta           = dot(rayDir, sunDir);
      float miePhaseValue      = miePhase(cosTheta);
      float rayleighPhaseValue = rayleighPhase(-cosTheta);

      vec3 lum = vec3(0.0), lumFactor = vec3(0.0), transmittance = vec3(1.0);
      for(int stepI = 1; stepI<=mulScattSteps; ++stepI) {
        float t = (float(stepI)/mulScattSteps)*tMax;
        float dt = tMax/mulScattSteps;

        vec3 newPos = pos + t*rayDir;

        vec3  rayleighScattering;
        vec3  extinction;
        float mieScattering;
        scatteringValues(newPos, rayleighScattering, mieScattering, extinction);

        vec3 sampleTransmittance = exp(-dt*extinction);

        // Integrate within each segment.
        vec3 scatteringNoPhase = rayleighScattering + mieScattering;
        vec3 scatteringF = (scatteringNoPhase - scatteringNoPhase * sampleTransmittance) / extinction;
        lumFactor += transmittance*scatteringF;

        // This is slightly different from the paper, but I think the paper has a mistake?
        // In equation (6), I think S(x,w_s) should be S(x-tv,w_s).
        vec3 sunTransmittance = textureLUT(tLUT, newPos, sunDir);

        vec3 rayleighInScattering = rayleighScattering*rayleighPhaseValue;
        float mieInScattering = mieScattering*miePhaseValue;
        vec3 inScattering = (rayleighInScattering + mieInScattering)*sunTransmittance;

        // Integrated scattering within path segment.
        vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

        lum += scatteringIntegral*transmittance;
        transmittance *= sampleTransmittance;
        }

      if(groundDist > 0.0) {
        vec3 hitPos = pos + groundDist*rayDir;
        if(dot(pos, sunDir) > 0.0) {
          hitPos = normalize(hitPos)*RPlanet;
          lum   += transmittance*GGroundAlbedo*textureLUT(tLUT, hitPos, sunDir);
          }
        }

      fms += lumFactor*invSamples;
      lumTotal += lum*invSamples;
      }
    }
  }

void main() {
  vec2  uv          = inPos*vec2(0.5)+vec2(0.5);
  float sunCosTheta = 2.0*uv.x - 1.0;
  float sunTheta    = safeacos(sunCosTheta);
  float height      = mix(RPlanet, RAtmos, uv.y);

  vec3 pos    = vec3(0.0, height, 0.0);
  vec3 sunDir = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));

  vec3 lum, f_ms;
  mulScattValues(pos, sunDir, lum, f_ms);

  // Equation 10 from the paper.
  vec3 psi = lum  / (1.0 - f_ms);
  outColor = vec4(psi, 1.0);
  }
