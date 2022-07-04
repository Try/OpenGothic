#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"

layout(binding = 0) uniform sampler2D tLUT;
layout(binding = 1) uniform sampler2D mLUT;
layout(binding = 2) uniform sampler2D skyLUT;

#if defined(FOG)
layout(binding = 3) uniform sampler2D depth;
#else
layout(binding = 3) uniform sampler2D textureDayL0;
layout(binding = 4) uniform sampler2D textureDayL1;
layout(binding = 5) uniform sampler2D textureNightL0;
layout(binding = 6) uniform sampler2D textureNightL1;
#endif

layout(location = 0) in  vec2 inPos;
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

vec3 atmosphereFog(vec2 uv) {
  // NOTE: 10.0 is not physical accurate, but dunno how to achive nice look without it
  return textureLod(skyLUT, uv, 0).rgb * 10.0;
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
  vec2 uv        = inPos*vec2(0.5)+vec2(0.5);
  vec3 view      = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir    = push.sunDir;
  vec3 pos       = vec3(0,RPlanet+push.plPosY,0);

#if defined(FOG)
  float z        = texture(depth,uv).r;
  vec3  pos1     = inverse(vec3(inPos,z));
  vec3  pos0     = inverse(vec3(inPos,0));

  float dist     = length(pos1-pos0);
  float fogDens  = volumetricFog(pos0,pos1-pos0);

  vec3  lum      = atmosphereFog(uv);
  lum *= GSunIntensity;
  lum = finalizeColor(lum,sunDir);

  vec3  fogColor = lum*fogDens;
  outColor       = vec4(fogColor,fogDens);
#else
  vec3 lum       = atmosphere(view, sunDir);
  vec3 sunLum    = sunWithBloom (view, sunDir);

  // Use smoothstep to limit the effect, so it drops off to actual zero.
  sunLum = smoothstep(0.002, 1.0, sunLum);
  sunLum *= textureLUT(tLUT, view, sunDir);
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

  // outColor  = texture(tLUT,uv);
  // outColor  = texture(mLUT,uv);
  // outColor  = texture(skyLUT,uv);
  outColor = vec4(lum,1.0);
#endif
  }
