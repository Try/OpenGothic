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

layout(push_constant, std430) uniform UboPush {
  mat4  viewProjectInv;
  vec2  cloudsDir0;
  vec2  cloudsDir1;
  vec3  sunDir;
  float night;
  vec3  clipInfo;
  float plPosY;
  float rayleighScatteringScale;
  float GSunIntensity;
  float exposure;
  } push;

vec3 inverse(vec3 pos) {
  vec4 ret = push.viewProjectInv*vec4(pos,1.0);
  return (ret.xyz/ret.w)/100.f;
  }

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

// 4. Atmospheric model
void scatteringValues(vec3 pos,
                      float clouds,
                      out vec3 rayleighScattering,
                      out float mieScattering,
                      out vec3 extinction) {
  float altitudeKM          = (length(pos)-RPlanet) / 1000.0;
  // Note: Paper gets these switched up. See SkyAtmosphereCommon.cpp:SetupEarthAtmosphere in demo app
  float rayleighDensity    = exp(-altitudeKM/8.0);
  float mieDensity         = exp(-altitudeKM/1.2);
  float ozoneDistribution  = max(0.0, 1.0 - abs(altitudeKM-25.0)/15.0);

  rayleighScattering       = rayleighScatteringBase*rayleighDensity*push.rayleighScatteringScale;
  float rayleighAbsorption = rayleighAbsorptionBase*rayleighDensity;

  mieScattering            = mieScatteringBase*mieDensity;
  float mieAbsorption      = mieAbsorptionBase*mieDensity;

  vec3  ozoneAbsorption    = ozoneAbsorptionBase*ozoneDistribution;

  // Clouds Ah-Hook
  mieScattering      *= exp( clouds*3.0); // (1.0+clouds*4.0);
  rayleighScattering *= exp(-clouds*3.0); // (1.0-clouds*0.5);

  extinction = rayleighScattering + rayleighAbsorption +
               mieScattering + mieAbsorption +
               ozoneAbsorption;
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
