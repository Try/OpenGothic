#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "sky_common.glsl"

layout(std140, push_constant) uniform Push {
  vec2 viewportSize;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D tLUT;
layout(binding  = 2) uniform sampler2D mLUT;
layout(binding  = 3) uniform sampler2D cloudsLUT;

layout(location = 0) out vec4 outColor;

const int numScatteringSteps = 32;

vec3 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax) {
  const float cosTheta      = dot(rayDir, sunDir);

  const float phaseMie      = miePhase(cosTheta);
  const float phaseRayleigh = rayleighPhase(-cosTheta);
  const float clouds        = textureLod(cloudsLUT, vec2(scene.isNight,0), 0).a;

  vec3  scatteredLight = vec3(0.0);
  vec3  transmittance  = vec3(1.0);

  for(int i=0; i<numScatteringSteps; ++i) {
    float t      = (float(i+0.3)/numScatteringSteps)*tMax;
    float dt     = tMax/numScatteringSteps;
    vec3  newPos = pos + t*rayDir;

    const ScatteringValues sc = scatteringValues(newPos, clouds);

    vec3 transmittanceSmp = exp(-dt*sc.extinction);
    vec3 transmittanceSun = textureLUT(tLUT, newPos, sunDir);
    vec3 psiMS            = textureLUT(mLUT, newPos, sunDir);

    vec3 scatteringSmp = vec3(0);
    scatteringSmp += psiMS * (sc.rayleighScattering + sc.mieScattering);
    scatteringSmp += sc.rayleighScattering * phaseRayleigh * transmittanceSun;
    scatteringSmp += sc.mieScattering      * phaseMie      * transmittanceSun;

    // Integrated scattering within path segment.
    vec3 scatteringIntegral = (scatteringSmp - scatteringSmp * transmittanceSmp) / sc.extinction;

    scatteredLight += scatteringIntegral*transmittance;
    transmittance  *= transmittanceSmp;
    }

  return scatteredLight;
  }

void main() {
  const vec2 uv       = vec2(gl_FragCoord.xy)/vec2(viewportSize);
  const vec3 viewPos  = vec3(0.0, RPlanet + scene.plPosY, 0.0);

  const float DirectSunLux  = scene.GSunIntensity;
  const float DirectMoonLux = GMoonIntensity;
  const vec3  moonInt       = NightAmbient/DirectSunLux;

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
  float sunAltitude = (0.5*M_PI) - acos(dot(scene.sunDir, up));

  vec3  sunDir      = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
  vec3  rayDir      = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));

  float atmoDist    = rayIntersect(viewPos, rayDir, RAtmos);
  float groundDist  = rayIntersect(viewPos, rayDir, RPlanet);
  float tMax        = (groundDist < 0.0) ? atmoDist : groundDist;

  vec3  sun  = raymarchScattering(viewPos, rayDir, sunDir, tMax);
  vec3  moon = raymarchScattering(viewPos, rayDir, normalize(vec3(-1,1,0)), tMax) * scene.isNight * (moonInt);
  outColor = vec4(sun+moon, 1.0);
  }
