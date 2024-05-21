#version 450

#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"
#include "sky_common.glsl"

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D tLUT;
layout(binding  = 2) uniform sampler2D mLUT;
layout(binding  = 3) uniform sampler2D cloudsLUT;
layout(binding  = 4) uniform texture2D depth;

layout(location = 0) out vec4 outColor;

const int numScatteringSteps = 32;

vec3 project(mat4 m, vec3 pos) {
  vec4 p = m*vec4(pos,1);
  return p.xyz/p.w;
  }
/*
vec3 applyClouds(vec3 skyColor) {
  const ivec2 dstSz  = textureSize(depth,0);
  const ivec2 dstUV  = ivec2(gl_FragCoord.xy);
  const vec2  inPos  = ((vec2(dstUV.xy)+vec2(0.5))/vec2(dstSz.xy))*2.0-vec2(1.0);
  vec3  pos1     = inverse(vec3(inPos,1.0));
  vec3  viewDir  = normalize(pos1);

  float night    = scene.isNight;
  vec3  plPos    = vec3(0,RPlanet+push.plPosY,0);
  return applyClouds(skyColor, skyLUT, plPos, scene.sunDir, viewDir, night,
                     scene.cloudsDir.xy, scene.cloudsDir.zw,
                     textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }*/

vec4 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax) {
  float cosTheta = dot(rayDir, sunDir);

  float miePhaseValue      = miePhase(cosTheta);
  float rayleighPhaseValue = rayleighPhase(-cosTheta);

  vec3  scatteredLight = vec3(0.0);
  vec3  transmittance  = vec3(1.0);

  const float clouds = textureLod(cloudsLUT, vec2(scene.isNight,0), 0).a;

  for(int i=1; i<=numScatteringSteps; ++i) {
    float t  = (float(i)/numScatteringSteps)*tMax;
    float dt = tMax/numScatteringSteps;

    vec3  newPos = pos + t*rayDir;

    vec3  rayleighScattering;
    vec3  extinction;
    float mieScattering;
    scatteringValues(newPos, clouds, rayleighScattering, mieScattering, extinction);

    vec3 sampleTransmittance = exp(-dt*extinction);

    vec3 sunTransmittance     = textureLUT(tLUT, newPos, sunDir);
    vec3 psiMS                = textureLUT(mLUT, newPos, sunDir);

    vec3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
    vec3 mieInScattering      = mieScattering     *(miePhaseValue*sunTransmittance      + psiMS);
    vec3 inScattering         = (rayleighInScattering + mieInScattering);

    // Integrated scattering within path segment.
    vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

    scatteredLight += scatteringIntegral*transmittance;
    transmittance  *= sampleTransmittance;
    }

  const float t = (transmittance.x+transmittance.y+transmittance.z)/3.0;
  return vec4(scatteredLight, t);
  }

void main() {
  const float DirectSunLux      = scene.GSunIntensity;
  const float DirectMoonLux     = 0.32f;
  const float moonInt           = DirectMoonLux/DirectSunLux;
  const float viewDistanceScale = 20;

  const ivec2 dstSz  = textureSize(depth,0);
  const ivec2 dstUV  = ivec2(gl_FragCoord.xy);
  const vec2  inPos  = ((vec2(dstUV.xy)+vec2(0.5))/vec2(dstSz.xy))*2.0-vec2(1.0);

  const float dMin   = 0;
  const float dMax   = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;
  const vec3  pos0   = project(scene.viewProjectInv, vec3(inPos,dMin));
  const vec3  pos1   = project(scene.viewProjectInv, vec3(inPos,dMax));
  const vec4  shPos0 = scene.viewShadow[1]*vec4(pos0, 1);
  const vec4  shPos1 = scene.viewShadow[1]*vec4(pos1, 1);

  const vec3  viewPos    = vec3(0.0, RPlanet + push.plPosY, 0.0);
  const vec3  rayDir     = normalize(pos1 - pos0);
  const vec3  sunDir     = scene.sunDir;

  const float atmoDist   = rayIntersect(viewPos, rayDir, RAtmos);
  const float groundDist = length(pos1-pos0)*0.01;  // meters
  const float tMax       = min(atmoDist, groundDist*viewDistanceScale);
  // const float groundDist = rayIntersect(viewPos, rayDir, RPlanet);
  // const float tMax       = (groundDist < 0.0) ? atmoDist : groundDist;

  const vec4  sun  = raymarchScattering(viewPos, rayDir, sunDir, tMax);
  const vec3  moon = raymarchScattering(viewPos, rayDir, vec3(0,1,0), tMax).rgb * scene.isNight * moonInt;

  outColor = vec4(sun.rgb + moon, dMax<1 ? sun.w : 0.0);
  }
