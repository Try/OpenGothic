#version 450

#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"
#include "sky_common.glsl"
#include "common.glsl"

layout(push_constant, std430) uniform UboPush {
  float rayleighScatteringScale;
  } push;
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D tLUT;
layout(binding  = 2) uniform sampler2D mLUT;
layout(binding  = 3) uniform sampler2D cloudsLUT;
layout(binding  = 4) uniform texture2D depth;
layout(binding  = 5) uniform sampler2D textureSm1;

layout(location = 0) out vec4 outColor;

const int  numScatteringSteps = 32;
vec3       viewOrigin         = vec3(0.0, RPlanet + scene.plPosY, 0.0);

float interleavedGradientNoise() {
  return interleavedGradientNoise(gl_FragCoord.xy);
  }

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

float shadowTest(vec3 pos) {
  // return 1;
  vec4  t  = scene.viewShadow[1]*vec4(pos, 1);
  //vec4 t = pos;
  t.xyz /= t.w;
  if(abs(t.x)>1 || abs(t.y)>1)
    return 1;

  vec2  xy = (t.xy)*0.5 + 0.5;
  float v  = textureLod(textureSm1, xy, 0).x;
  return (v < t.z) ? 1 : 0;
  }

vec4 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax, vec3 smPos, float tSmMax) {
  const float cosTheta      = dot(rayDir, sunDir);
  const float noise         = interleavedGradientNoise()/numScatteringSteps;

  const float phaseMie      = miePhase(cosTheta);
  const float phaseRayleigh = rayleighPhase(-cosTheta);
  const float clouds        = textureLod(cloudsLUT, vec2(scene.isNight,0), 0).a;

  vec3  scatteredLight = vec3(0.0);
  vec3  transmittance  = vec3(1.0);

  for(int i=0; i<numScatteringSteps; ++i) {
    float t      = (float(i+0.3)/numScatteringSteps)*tMax;
    float dt     = tMax/numScatteringSteps;
    vec3  newPos = pos + t*rayDir;

    float tSm        = (float(i+noise)/numScatteringSteps)*tSmMax;
    float visibility = shadowTest(smPos + tSm*rayDir*100.0);

    const ScatteringValues sc = scatteringValues(newPos, clouds, push.rayleighScatteringScale);

    vec3 transmittanceSmp = exp(-dt*sc.extinction);
    vec3 transmittanceSun = textureLUT(tLUT, newPos, sunDir);
    vec3 psiMS            = textureLUT(mLUT, newPos, sunDir);

    vec3 scatteringSmp = vec3(0);
    scatteringSmp += psiMS * (sc.rayleighScattering + sc.mieScattering);
    scatteringSmp += sc.rayleighScattering * phaseRayleigh * transmittanceSun * visibility;
    scatteringSmp += sc.mieScattering      * phaseMie      * transmittanceSun * visibility;

    // Integrated scattering within path segment.
    // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
    vec3 scatteringIntegral = (scatteringSmp - scatteringSmp * transmittanceSmp) / sc.extinction;

    scatteredLight += scatteringIntegral*transmittance;
    transmittance  *= transmittanceSmp;
    }

  const float t = (transmittance.x+transmittance.y+transmittance.z)/3.0;
  return vec4(scatteredLight, t);
  }

void main() {
  const float DirectSunLux      = scene.GSunIntensity;
  const float DirectMoonLux     = 0.32f;
  const float viewDistanceScale = 50;

  const ivec2 dstSz  = textureSize(depth,0);
  const ivec2 dstUV  = ivec2(gl_FragCoord.xy);
  const vec2  inPos  = ((vec2(dstUV.xy)+vec2(0.5))/vec2(dstSz.xy))*2.0-vec2(1.0);

  const float dMin   = 0;
  const float dMax   = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;
  const bool  isSky  = (dMax==1.0);
  const vec3  pos0   = project(scene.viewProjectLwcInv, vec3(inPos,dMin));
  const vec3  pos1   = project(scene.viewProjectLwcInv, vec3(inPos,dMax));
  // const vec4  shPos0 = scene.viewShadow[1]*vec4(pos0, 1);
  // const vec4  shPos1 = scene.viewShadow[1]*vec4(pos1, 1);

  const vec3  viewPos    = viewOrigin;
  const vec3  rayDir     = normalize(pos1 - pos0);
  const vec3  sunDir     = scene.sunDir;

  const float planetDist = rayIntersect(viewPos, rayDir, RPlanet);
  const float atmoDist   = planetDist<0 ? rayIntersect(viewPos, rayDir, RAtmos) : planetDist;
  const float groundDist = length(pos1-pos0)*0.01;  // meters
  const float tMax       = isSky ? atmoDist : min(atmoDist, groundDist*viewDistanceScale);
  const float tSmMax     = isSky ? atmoDist : min(atmoDist, groundDist);

  const vec4  sun  = raymarchScattering(viewPos, rayDir, sunDir,      tMax, pos0, tSmMax);
  const vec3  moon = vec3(0); //raymarchScattering(viewPos, rayDir, vec3(0,1,0), tMax, pos0, 0).rgb * scene.isNight;

  const vec3  luminance = (DirectSunLux*sun.rgb + DirectMoonLux*moon);

  outColor = vec4(luminance*scene.exposure, dMax<1 ? sun.w : 0.0);
  if(isnan(sun.r)) {
    // debug
    outColor = vec4(1,0,0,1);
    }
  }
