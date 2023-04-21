#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"

layout(binding = 0) uniform sampler2D tLUT;
layout(binding = 1) uniform sampler2D mLUT;
layout(binding = 2) uniform sampler2D skyLUT;

layout(binding = 4) uniform sampler2D textureDayL0;
layout(binding = 5) uniform sampler2D textureDayL1;
layout(binding = 6) uniform sampler2D textureNightL0;
layout(binding = 7) uniform sampler2D textureNightL1;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

vec4 clouds(vec3 at, float nightPhase, vec3 highlight,
            vec2 dxy0, vec2 dxy1,
            in sampler2D dayL1,   in sampler2D dayL0,
            in sampler2D nightL1, in sampler2D nightL0) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

  vec4  cloudDL1 = texture(dayL1,   texc*0.3 + dxy1);
  vec4  cloudDL0 = texture(dayL0,   texc*0.3 + dxy0);
  vec4  cloudNL1 = texture(nightL1, texc*0.3 + dxy1);
  vec4  cloudNL0 = texture(nightL0, texc*0.6 + vec2(0.5)); // stars

  cloudDL0.a   = cloudDL0.a*0.2;
  cloudDL1.a   = cloudDL1.a*0.2;
  cloudNL1.a   = cloudNL1.a*0.1;

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb);
  night.rgb = srgbDecode(night.rgb);

  day.rgb   = day.rgb  *highlight;
  night.rgb = night.rgb*highlight*16.0;

  //day  .a   = day  .a*0.2;
  night.a   = night.a*(nightPhase);

  vec4 color = mixClr(day,night);
  // color.rgb += hday;

  return color;
  }

vec4 clouds(vec3 at, vec3 highlight) {
  return clouds(at, push.night, highlight,
                push.dxy0, push.dxy1,
                textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

/*
 * Final output basically looks up the value from the skyLUT, and then adds a sun on top,
 * does some tonemapping.
 */
vec3 textureSkyLUT(vec3 rayDir, vec3 sunDir) {
  const vec3  viewPos = vec3(0.0, RPlanet + push.plPosY, 0.0);
  return textureSkyLUT(skyLUT, viewPos, rayDir, sunDir);
  }

vec3 atmosphere(vec3 view, vec3 sunDir) {
  return textureSkyLUT(view, sunDir);
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

    vec3  rayleighScattering = vec3(0);
    vec3  extinction         = vec3(0);
    float mieScattering      = float(0);
    scatteringValues(newPos, 0, rayleighScattering, mieScattering, extinction);

    transmittance *= exp(-dt*extinction);
    }
  return transmittance;
  }

vec3 transmittanceAprox(in vec3 pos0, in vec3 pos1) {
  vec3 dir = pos1-pos0;

  vec3  rayleighScattering = vec3(0);
  float mieScattering      = float(0);
  vec3  extinction0        = vec3(0);
  vec3  extinction1        = vec3(0);
  scatteringValues(pos0 + vec3(0,RPlanet,0), 0, rayleighScattering, mieScattering, extinction0);
  scatteringValues(pos1 + vec3(0,RPlanet,0), 0, rayleighScattering, mieScattering, extinction1);

  vec3  extinction         = extinction1;//-extinction0;
  return exp(-length(dir)*extinction);
  }

vec3 sky(vec2 uv, vec3 sunDir) {
  vec3  pos  = vec3(0,RPlanet,0);
  vec3  pos1 = inverse(vec3(inPos,1.0));
  vec3  pos0 = inverse(vec3(inPos,0));

  vec3 view  = normalize(pos1);
  vec3 lum   = atmosphere(view, sunDir);
  return lum;
  }

vec3 applyClouds(vec3 skyColor, vec3 sunDir) {
  vec3  pos      = vec3(0,RPlanet+push.plPosY,0);
  vec3  pos1     = inverse(vec3(inPos,1.0));
  vec3  view     = normalize(pos1);

  float L        = rayIntersect(pos, view, RClouds);
  // TODO: http://killzone.dl.playstation.net/killzone/horizonzerodawn/presentations/Siggraph15_Schneider_Real-Time_Volumetric_Cloudscapes_of_Horizon_Zero_Dawn.pdf
  // fake cloud scattering inspired by Henyey-Greenstein model
  vec3  lum      = vec3(0);
  lum += atmosphere  (vec3( view.x, view.y*0.0, view.z), sunDir);
  lum += atmosphere  (vec3(-view.x, view.y*0.0, view.z), sunDir);
  lum += atmosphere  (vec3(-view.x, view.y*0.0,-view.z), sunDir);
  lum += atmosphere  (vec3( view.x, view.y*0.0,-view.z), sunDir);
  lum = lum*push.GSunIntensity;
  //return lum;

  vec4  cloud    = clouds(pos + view*L, lum);

  return mix(skyColor, cloud.rgb, cloud.a);
  }

void main() {
  vec2 uv     = inPos*vec2(0.5)+vec2(0.5);
  vec3 view   = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir = push.sunDir;

  // Sky
  vec3  lum = sky(uv, sunDir);
  float tr  = 1.0;
  // Clouds
  lum = applyClouds(lum, sunDir);
  lum = lum * push.GSunIntensity;

  outColor = vec4(lum, tr);
  }
