#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"

layout(binding = 0) uniform sampler2D tLUT;
layout(binding = 1) uniform sampler2D mLUT;
layout(binding = 2) uniform sampler2D skyLUT;

#if defined(VOLUMETRIC)
layout(binding = 3) uniform sampler3D fogLut;
#endif

layout(binding = 4) uniform sampler2D depth;
layout(binding = 5) uniform sampler2D textureDayL0;
layout(binding = 6) uniform sampler2D textureDayL1;
layout(binding = 7) uniform sampler2D textureNightL0;
layout(binding = 8) uniform sampler2D textureNightL1;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

#if !defined(FOG)
vec4 clouds(vec3 at, float nightPhase, float GSunIntensity, vec3 highlight,
            vec2 dxy0, vec2 dxy1,
            in sampler2D dayL1,   in sampler2D dayL0,
            in sampler2D nightL1, in sampler2D nightL0) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

  vec4  cloudDL1 = texture(dayL1,   texc*0.3 + dxy1);
  vec4  cloudDL0 = texture(dayL0,   texc*0.3 + dxy0);
  vec4  cloudNL1 = texture(nightL1, texc*0.3 + dxy1);
  vec4  cloudNL0 = texture(nightL0, texc*0.6 + vec2(0.5)); // stars

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb)*push.GSunIntensity*1.0;
  day.rgb   = day.rgb*highlight*1.0;
  night.rgb = srgbDecode(night.rgb);

  //day  .a   = day  .a*(1.0-nightPhase);
  day  .a   = day  .a*0.1;
  night.a   = night.a*(nightPhase);

  vec4 color = mixClr(day,night);
  // color.rgb += hday;

  return color;
  }

vec4 clouds(vec3 at, vec3 highlight) {
  return clouds(at, push.night, push.GSunIntensity, highlight,
                push.dxy0, push.dxy1,
                textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

#endif

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

vec3 finalizeColor(vec3 color, vec3 sunDir) {
  // Tonemapping and gamma. Super ad-hoc, probably a better way to do this.
  color = pow(color, vec3(1.3));
  color /= (smoothstep(0.0, 0.2, clamp(sunDir.y, 0.0, 1.0))*2.0 + 0.15);

  color = reinhardTonemap(color);
  // color = acesTonemap(color);

  color = srgbEncode(color);
  return color;
  }

// debug only
vec3 transmittance(vec3 pos0, vec3 pos1) {
  const int   steps = 32;

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
    scatteringValues(newPos, rayleighScattering, mieScattering, extinction);

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
  scatteringValues(pos0 + vec3(0,RPlanet,0), rayleighScattering, mieScattering, extinction0);
  scatteringValues(pos1 + vec3(0,RPlanet,0), rayleighScattering, mieScattering, extinction1);

  vec3  extinction         = extinction1;//-extinction0;
  return exp(-length(dir)*extinction);
  }

// based on: https://developer.amd.com/wordpress/media/2012/10/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf
float volumetricFog(in vec3 pos, in vec3 pos1) {
  vec3 dir = pos1-pos;
  // Fog props
  const float fogHeightDensityAtViewer = 0.5;
  const float globalDensity            = 0.005;
  const float heightFalloff            = 0.02;

  // float fogHeightDensityAtViewer = exp(-heightFalloff * pos.z);
  float fogInt = length(dir) * fogHeightDensityAtViewer;
  if(abs(dir.y) > 0.01) {
    float t = heightFalloff*dir.y;
    fogInt *= (1.0-exp(-t))/t;
    }

  return exp(-globalDensity*fogInt);
  }

#if defined(VOLUMETRIC)
#define MAX_DEBUG_COLORS 10
const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };

vec4 fog(vec2 uv, float z, vec3 sunDir) {
  float dMin = 0;
  float dMax = 0.9999;

  float dZ   = reconstructCSZ(   z, push.clipInfo);
  float d0   = reconstructCSZ(dMin, push.clipInfo);
  float d1   = reconstructCSZ(dMax, push.clipInfo);

  float d    = (dZ-d0)/(d1-d0);
  // return vec4(debugColors[min(int(d*textureSize(fogLut,0).z), textureSize(fogLut,0).z-1)%MAX_DEBUG_COLORS], 1);

  // vec3  trans    = transmittance(pos0, posz);
  // vec3  trans    = transmittance(pos0, pos1);

  vec4  val      = textureLod(fogLut, vec3(uv,d), 0);
  vec3  trans    = vec3(1.0-val.w);
  float fogDens  = (trans.x+trans.y+trans.z)/3.0;

  vec3  lum      = val.rgb * push.GSunIntensity;
  return vec4(lum, fogDens);
  }
#else
vec4 fog(vec2 uv, float z, vec3 sunDir) {
  float dMin = 0.0;
  float dMax = 1.0;

  float dZ   = reconstructCSZ(   z, push.clipInfo);
  float d0   = reconstructCSZ(dMin, push.clipInfo);
  float d1   = reconstructCSZ(dMax, push.clipInfo);

  float d    = (dZ-d0)/(d1-d0);

  vec3  pos0     = inverse(vec3(inPos,0));
  vec3  posz     = inverse(vec3(inPos,z));

  vec3  val      = textureLod(skyLUT, uv, 0).rgb;
  //vec3  trans    = vec3(1.0)-transmittance(pos0, posz);
  vec3  trans    = vec3(1.0)-transmittanceAprox(pos0, posz);
  float fogDens  = (trans.x+trans.y+trans.z)/3.0;

  //return vec4(fogDens);

  vec3  lum      = val.rgb * push.GSunIntensity;
  lum *= clamp(d,0,1);
#if !defined(FOG)
  lum = vec3(0);
#endif
  return vec4(lum, fogDens);
  }
#endif

vec3 sky(vec2 uv, vec3 sunDir) {
  vec3  pos      = vec3(0,RPlanet+push.plPosY,0);
  vec3  pos1     = inverse(vec3(inPos,1.0));
  vec3  pos0     = inverse(vec3(inPos,0));

  vec3 view      = normalize(pos1);
  vec3 lum       = atmosphere  (view, sunDir);
  vec3 sunLum    = sunWithBloom(view, sunDir);

  // Use smoothstep to limit the effect, so it drops off to actual zero.
  sunLum = smoothstep(0.002, 1.0, sunLum);
  sunLum *= textureLUT(tLUT, view, sunDir);

  if((sunLum.x>0 || sunLum.y>0 || sunLum.z>0) && rayIntersect(pos, view, RPlanet)>=0.0) {
    sunLum = vec3(0.0);
    }
  lum += sunLum;
  lum *= push.GSunIntensity;
  return lum;
  }

#if !defined(FOG)
vec3 applyClouds(vec3 skyColor, vec3 sunDir) {
  vec3  pos      = vec3(0,RPlanet+push.plPosY,0);
  vec3  pos1     = inverse(vec3(inPos,1.0));
  vec3  view     = normalize(pos1);

  float L        = rayIntersect(pos, view, RClouds);
  // TODO: http://killzone.dl.playstation.net/killzone/horizonzerodawn/presentations/Siggraph15_Schneider_Real-Time_Volumetric_Cloudscapes_of_Horizon_Zero_Dawn.pdf\
  // fake cloud scattering inspired by Henyey-Greenstein model
  vec3  lum      = vec3(0);
  lum += atmosphere  (vec3( view.x, view.y*0.0, view.z), sunDir);
  lum += atmosphere  (vec3(-view.x, view.y*0.0, view.z), sunDir);
  lum += atmosphere  (vec3(-view.x, view.y*0.0,-view.z), sunDir);
  lum += atmosphere  (vec3( view.x, view.y*0.0,-view.z), sunDir);
  lum = lum*push.GSunIntensity;
  //return lum;

  //vec4  cloud    = clouds(pos + view*L, vec3(push.GSunIntensity));
  vec4  cloud    = clouds(pos + view*L, lum);
  // cloud.rgb *= textureLUT(tLUT, view, sunDir);

  return mix(skyColor, cloud.rgb, cloud.a);
  }
#endif

void main() {
  vec2 uv     = inPos*vec2(0.5)+vec2(0.5);
  vec3 view   = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir = push.sunDir;
  float z     = textureLod(depth,uv,0).r;

#if defined(FOG)
  if(z>=1.0)
    discard;
#endif

  // NOTE: not a physical value, but dunno how to achive nice look without it
  float fogFixup = 20.0;

  vec4  val      = fog(uv,z,push.sunDir) * fogFixup;
  vec3  lum      = val.rgb;
#if defined(FOG)
  //outColor = fog(uv, sunDir);
  //return;
#endif

#if !defined(FOG)
  // Sky
  // lum = sky(uv,push.sunDir);
  lum = lum*0.5 + sky(uv, sunDir);
  // Clouds
  lum = applyClouds(lum, sunDir);
#endif

  lum      = finalizeColor(lum, sunDir);
  //lum = vec3(val.a); //debug transmittance
  outColor = vec4(lum, val.a);
#if !defined(FOG)
  outColor.a = 1.0;
#endif

  // outColor = vec4(val.a*2.0);
  }
