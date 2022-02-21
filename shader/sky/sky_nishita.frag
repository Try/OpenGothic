#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#define FRAGMENT
#include "sky_common.glsl"

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
vec4 cloudsDay(vec2 texc){
  vec4 cloudDL1 = texture(textureDayL1,texc*0.3+push.dxy1);
  vec4 cloudDL0 = texture(textureDayL0,texc*0.3+push.dxy0);

#ifdef G1
  return mix(cloudDL0,cloudDL1,cloudDL1.a);
#else
  return (cloudDL0+cloudDL1);
#endif
  }

vec4 cloudsNight(vec2 texc){
  vec4 cloudNL1 = texture(textureNightL1,texc*0.3+push.dxy1);
  vec4 cloudNL0 = texture(textureNightL0,texc*0.6);
#ifdef G1
  vec4 night    = mix(cloudNL0,cloudNL1,cloudNL1.a);
#else
  vec4 night    = cloudNL0+cloudNL1;
#endif
  return vec4(night.rgb,push.night);
  }

vec4 clouds(vec3 at) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));
  vec4  day      = cloudsDay  (texc);
  vec4  night    = cloudsNight(texc);
  return  mix(day,night,push.night);
  }
#endif

vec3 inverse(vec3 pos) {
  vec4 ret = push.viewProjectInv*vec4(pos,1.0);
  return (ret.xyz/ret.w)/100.f;
  }

void main() {
  vec3  view     = normalize(inverse(vec3(inPos,1.0)));
  vec3  sunDir   = push.sunDir;
  vec3  pos      = vec3(0,RPlanet+push.plPosY,0);

#if defined(FOG)
  vec2  uv       = inPos*0.5+vec2(0.5);
  float z        = texture(depth,uv).r;
  vec3 pos1      = inverse(vec3(inPos,z));
  vec3 pos0      = inverse(vec3(inPos,0));

  float dist     = length(pos1-pos0);
  float fogDens  = volumetricFog(pos0,pos1-pos0);

  vec3  fogColor = skyColor*fogDens;
  fogColor       = exposure(fogColor);
  outColor       = vec4(fogColor,fogDens);
#else
  // Sky
  vec3  atmo    = atmosphere(pos,view,sunDir);

  // Sun
  float alpha    = dot(view,sunDir);
  float spot     = smoothstep(0.0, 1000.0, phase(alpha, 0.9995));
  vec3  sun      = vec3(spot*1000.0);

  // Apply exposure.
  vec3  color    = atmo + sun;
  color          = exposure(color);

  float L        = rayIntersect(pos, view, RClouds);
  // Clouds
  vec4  cloud    = clouds(pos + view*L);
  // Fog
  float fogDens  = volumetricFog(pos, view*L);
  color          = mix(color,cloud.rgb,min(1.0,cloud.a*(1.0-fogDens)));

  outColor       = vec4(color,1.0);
#endif
  }
