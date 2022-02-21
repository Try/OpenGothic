#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky_common.glsl"

layout(location = 0) in  vec2 inPos;

layout(location = 0) out vec4 outColor;

const int sunTransmittanceSteps = 40;

vec3 sunTransmittance(vec3 pos, vec3 sunDir) {
  if(rayIntersect(pos, sunDir, RPlanet) > 0.0)
    return vec3(0.0);

  float atmoDist = rayIntersect(pos, sunDir, RAtmos);
  float t = 0.0;

  vec3 transmittance = vec3(1.0);
  for(int i=0; i<sunTransmittanceSteps; ++i) {
    float t  = ((float(i) + 0.3)/sunTransmittanceSteps)*atmoDist;
    float dt = atmoDist/sunTransmittanceSteps;

    vec3 newPos = pos + t*sunDir;

    vec3  rayleighScattering = vec3(0);
    vec3  extinction         = vec3(0);
    float mieScattering      = float(0);
    scatteringValues(newPos, rayleighScattering, mieScattering, extinction);

    transmittance *= exp(-dt*extinction);
    }
  return transmittance;
  }

void main() {
  vec2  uv          = inPos*vec2(0.5)+vec2(0.5);
  float sunCosTheta = 2.0*uv.x - 1.0;
  float sunTheta    = safeacos(sunCosTheta);
  float height      = mix(RPlanet, RAtmos, uv.y);

  vec3  pos         = vec3(0.0, height, 0.0);
  vec3  sunDir      = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));

  outColor          = vec4(sunTransmittance(pos, sunDir), 1.0);
  }
