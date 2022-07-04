#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#define FRAGMENT
#include "sky_common.glsl"

#if defined(FOG)
const int   iSteps = 8;
const int   jSteps = 8;
#else
const int   iSteps = 8;
const int   jSteps = 16;
#endif

#if defined(FOG)
layout(binding = 1) uniform sampler2D depth;
#else
layout(binding = 1) uniform sampler2D textureDayL0;
layout(binding = 2) uniform sampler2D textureDayL1;
layout(binding = 3) uniform sampler2D textureNightL0;
layout(binding = 4) uniform sampler2D textureNightL1;
#endif

layout(location = 0) in  vec2 inPos;
layout(location = 1) in  vec3 skyColor;

layout(location = 0) out vec4 outColor;

#if !defined(FOG)
vec4 clouds(vec3 at) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

  vec4  cloudDL1 = texture(textureDayL1,  texc*0.3+push.dxy1);
  vec4  cloudDL0 = texture(textureDayL0,  texc*0.3+push.dxy0);
  vec4  cloudNL1 = texture(textureNightL1,texc*0.3+push.dxy1);
  vec4  cloudNL0 = texture(textureNightL0,texc*0.6+vec2(0.5)); // stars

#ifdef G1
  vec4 day       = mix(cloudDL0,cloudDL1,cloudDL1.a);
  vec4 night     = mix(cloudNL0,cloudNL1,cloudNL1.a);
#else
  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;
#endif

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb)*GSunIntensity;
  day.a     = day.a*(1.0-push.night);

  night.rgb = srgbDecode(night.rgb);
  night.a   = night.a*(push.night);

  vec4 color = mixClr(day,night);
  return color;
  }
#endif

vec2 densities(in vec3 pos) {
  const float haze = 0.002;

  float altitudeKM      = (length(pos)-RPlanet) / 1000.0;
  // Note: Paper gets these switched up. See SkyAtmosphereCommon.cpp:SetupEarthAtmosphere in demo app
  float rayleighDensity = exp(-altitudeKM/8.0);
  float mieScattering   = exp(-altitudeKM/1.2) + haze;

  return vec2(rayleighDensity,mieScattering);
  }

// based on: http://www.scratchapixel.com/lessons/3d-advanced-lessons/simulating-the-colors-of-the-sky/atmospheric-scattering/
// https://www.shadertoy.com/view/ltlSWB
vec3 scatter(vec3 pos, vec3 view, vec3 sunDir, float rayLength, out float scat) {
  float cosTheta           = dot(view, sunDir);
  float miePhaseValue      = miePhase(cosTheta);
  float rayleighPhaseValue = rayleighPhase(-cosTheta);

  vec3 R = vec3(0.0), M = vec3(0.0);

  vec2 depth = vec2(0);
  float dl = rayLength / float(iSteps);
  for(int i = 1; i <= iSteps; ++i) {
    float l = float(i) * dl;
    vec3 p = pos + view * l;

    vec2 dens = densities(p)*dl;
    depth += dens;

    float Ls = rayIntersect(p, sunDir, RAtmos);
    if(Ls>0.0) {
      float dls = Ls / float(jSteps);
      vec2 depthS = vec2(0);
      for(int j = 1; j <= jSteps; ++j) {
        float ls = float(j) * dls;
        vec3  ps = p + sunDir * ls;
        depthS  += densities(ps)*dls;
        }

      vec3 A = exp(-(rayleighScatteringBase * (depthS.x + depth.x) + mieScatteringBase * (depthS.y + depth.y)));
      R += A * dens.x;
      M += A * dens.y;
    }
  }

  scat = 1.0 - clamp(depth.y*1e-5,0.,1.);
  return (R * rayleighScatteringBase * rayleighPhaseValue + M * mieScatteringBase * miePhaseValue);
  }

vec3 atmosphere(in vec3 pos, vec3 view, vec3 sunDir, float rayLength) {
  // moon
  float att = 1.0;
  if(sunDir.y < -0.0) {
    sunDir.y = -sunDir.y;
    att = 0.15;
    }
  view.y = abs(view.y);
  float scatt     = 0;
  return scatter(pos, view, sunDir, rayLength, scatt) * att;
  }

vec3 finalizeColor(vec3 color, vec3 sunDir) {
  // Tonemapping and gamma. Super ad-hoc, probably a better way to do this.
  color = pow(color, vec3(1.3));
  color /= (smoothstep(0.0, 0.2, clamp(sunDir.y, 0.0, 1.0))*2.0 + 0.15);
  color = jodieReinhardTonemap(color);
  color = srgbEncode(color);
  return color;
  }

void main() {
  vec3 view      = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir    = push.sunDir;
  vec3 pos       = vec3(0,RPlanet+push.plPosY,0);

#if defined(FOG)
  vec2 uv        = inPos*vec2(0.5)+vec2(0.5);
  float z        = texture(depth,uv).r;
  vec3  pos1     = inverse(vec3(inPos,z));
  vec3  pos0     = inverse(vec3(inPos,0));

  float dist     = length(pos1-pos0);
  float fogDens  = volumetricFog(pos0,pos1-pos0);

  vec3  lum      = atmosphere(pos,view,sunDir,fogFarDistance);
  lum *= GSunIntensity;
  lum = finalizeColor(lum,sunDir);

  vec3  fogColor = lum*fogDens;
  outColor       = vec4(fogColor,fogDens);
#else
  float rayLength = rayIntersect(pos, view, RAtmos);
  vec3 lum        = atmosphere(pos, view, sunDir, rayLength);
  vec3 sunLum     = sunWithBloom (view, sunDir);

  // Use smoothstep to limit the effect, so it drops off to actual zero.
  sunLum = smoothstep(0.002, 1.0, sunLum);
  //sunLum *= textureLUT(tLUT, view, sunDir);
  if((sunLum.x>0 || sunLum.y>0 || sunLum.z>0) && rayIntersect(pos, view, RPlanet)>=0.0) {
    sunLum = vec3(0.0);
    }
  lum += sunLum;
  lum *= GSunIntensity;

  float L       = rayIntersect(pos, view, RClouds);
  // Clouds
  vec4  cloud   = clouds(pos + view*L);
  // Fog
  float fogDens = volumetricFog(pos, view*L);
  lum           = mix(lum,cloud.rgb,min(1.0,cloud.a*(1.0-fogDens)));

  lum = finalizeColor(lum,sunDir);
  outColor = vec4(lum,1.0);
#endif
  }
