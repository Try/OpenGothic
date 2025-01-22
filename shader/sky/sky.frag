#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"
#include "scene.glsl"
#include "clouds.glsl"

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D tLUT;
layout(binding = 2) uniform sampler2D mLUT;
layout(binding = 3) uniform sampler2D skyLUT;
layout(binding = 4) uniform sampler3D fogLut;

layout(binding = 5) uniform sampler2D textureDayL0;
layout(binding = 6) uniform sampler2D textureDayL1;
layout(binding = 7) uniform sampler2D textureNightL0;
layout(binding = 8) uniform sampler2D textureNightL1;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

vec3 inverse(vec3 pos) {
  vec4 ret = scene.viewProjectLwcInv*vec4(pos,1.0);
  return (ret.xyz/ret.w)/100.f;
  }

vec3 atmosphere(vec3 view, vec3 sunDir) {
  const vec3  viewPos = vec3(0.0, RPlanet + scene.plPosY, 0.0);
  return textureSkyLUT(skyLUT, viewPos, view, sunDir);
  }

// debug only
vec3 transmittance(vec3 pos0, vec3 pos1) {
  const int steps = 32;

  vec3  transmittance = vec3(1.0);
  vec3  dir  = pos1-pos0;
  float dist = length(dir);
  for(int i=1; i<=steps; ++i) {
    float t      = (float(i)/steps);
    float dt     = dist/steps;
    vec3  newPos = pos0 + t*dir + vec3(0,RPlanet,0);

    const ScatteringValues sc = scatteringValues(newPos, 0);
    transmittance *= exp(-dt*sc.extinction);
    }
  return transmittance;
  }

vec3 transmittanceAprox(in vec3 pos0, in vec3 pos1) {
  vec3 dir = pos1-pos0;

  const ScatteringValues sc0 = scatteringValues(pos0 + vec3(0,RPlanet,0), 0);
  const ScatteringValues sc1 = scatteringValues(pos1 + vec3(0,RPlanet,0), 0);

  vec3  extinction = sc1.extinction;//-extinction0;
  return exp(-length(dir)*sc0.extinction);
  }

vec3 sky(vec2 uv, vec3 sunDir) {
  vec3  pos  = vec3(0,RPlanet,0);
  vec3  pos1 = inverse(vec3(inPos,1.0));
  vec3  pos0 = inverse(vec3(inPos,0));

  vec3 view  = normalize(pos1);
  vec3 lum   = atmosphere(view, sunDir);
  return lum;
  }

vec3 applyClouds(vec3 skyColor) {
  float night    = scene.isNight;
  vec3  plPos    = vec3(0, RPlanet + scene.plPosY, 0);
  vec3  pos1     = inverse(vec3(inPos,1.0));
  vec3  viewDir  = normalize(pos1);
  return applyClouds(skyColor, skyLUT, plPos, scene.sunDir, viewDir, night,
                     scene.cloudsDir.xy, scene.cloudsDir.zw,
                     textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

void main() {
  vec2 uv      = inPos*vec2(0.5)+vec2(0.5);
  vec3 view    = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir  = scene.sunDir;

  // accounted in additive fog later
  vec3  maxFog = textureLod(fogLut, vec3(uv, textureSize(fogLut,0).z-1), 0).rgb;
  // Sky
  vec3  lum = sky(uv, sunDir) - maxFog;
  float tr  = 1.0;
  // Clouds
  lum = applyClouds(lum);
  lum = lum * scene.GSunIntensity;

  lum *= scene.exposure;
  outColor = vec4(lum, tr);
  }
