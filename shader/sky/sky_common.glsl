#ifndef SKY_COMMON_GLSL
#define SKY_COMMON_GLSL

#include "common.glsl"
#include "lighting/tonemapping.glsl"

// Table 1: Coefficients of the different participating media compo-nents constituting the Earth’s atmosphere
// These are per megameter.
const vec3  rayleighScatteringBase = vec3(0.175, 0.409, 1.0) / 1e6;
const float rayleighAbsorptionBase = 0.0;
const float g                      = 0.8;          // light concentration .76 //.45 //.6  .45 is normaL

const float mieScatteringBase      = 3.996 / 1e6;
const float mieAbsorptionBase      = 4.40  / 1e6;

// NOTE: Ozone does not contribute to scattering; it only absorbs light.
const vec3  ozoneAbsorptionBase    = vec3(0.650, 1.881, .085) / 1e6;

const float dFogMin = 0;
const float dFogMax = 0.9999;

float miePhase(float cosTheta) {
  const float scale = 3.0/(8.0*M_PI);

  float num   = (1.0-g*g)*(1.0+cosTheta*cosTheta);
  float denom = (2.0+g*g)*pow((1.0 + g*g - 2.0*g*cosTheta), 1.5);

  return scale*num/denom;
  }

float rayleighPhase(float cosTheta) {
  const float k = 3.0/(16.0*M_PI);
  return k*(1.0+cosTheta*cosTheta);
  }

struct ScatteringValues {
  vec3  rayleighScattering;
  float mieScattering;
  vec3  extinction;
  };
// 4. Atmospheric model
ScatteringValues scatteringValues(float altitudeKM, float clouds, float rayleighScatteringScale) {
  // Note: Paper gets these switched up. See SkyAtmosphereCommon.cpp:SetupEarthAtmosphere in demo app
  float rayleighDensity    = exp(-altitudeKM/8.0);
  float mieDensity         = exp(-altitudeKM/1.2);
  float ozoneDistribution  = max(0.0, 1.0 - abs(altitudeKM-25.0)/15.0);

  ScatteringValues ret;
  ret.rayleighScattering   = rayleighScatteringBase*rayleighDensity*rayleighScatteringScale;
  float rayleighAbsorption = rayleighAbsorptionBase*rayleighDensity;

  ret.mieScattering        = mieScatteringBase*mieDensity;
  float mieAbsorption      = mieAbsorptionBase*mieDensity;

  vec3  ozoneAbsorption    = ozoneAbsorptionBase*ozoneDistribution;

  // Clouds Ah-Hook
  clouds = max(0, clouds-0.2); // 0.33 in LH
  ret.mieScattering      *= exp( clouds*5.0);
  ret.rayleighScattering *= exp(-clouds*5.0);

  ret.extinction = ret.rayleighScattering + rayleighAbsorption +
                   ret.mieScattering + mieAbsorption +
                   ozoneAbsorption;
  return ret;
  }

ScatteringValues scatteringValues(vec3 pos, float clouds, float rayleighScatteringScale) {
  float altitudeKM = (length(pos)-RPlanet) / 1000.0;
  return scatteringValues(altitudeKM, clouds, rayleighScatteringScale);
  }

// 5.5.2. LUT parameterization
vec3 textureLUT(sampler2D tex, vec3 pos, vec3 sunDir) {
  float height   = length(pos);
  vec3  up       = pos / height;
  float cosAngle = dot(sunDir, up);

  vec2 uv;
  uv.x = 0.5 + 0.5*cosAngle;
  uv.y = (height - RPlanet)/(RAtmos - RPlanet);
  return texture(tex, uv).rgb;
  }

#endif
