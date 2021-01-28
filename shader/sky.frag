#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#define FRAGMENT
#include "sky_common.glsl"

layout(location = 0) in  vec2 inPos;
layout(location = 1) in  vec3 skyColor;

layout(location = 0) out vec4 outColor;

// Fog props
const float fogHeightDensityAtViewer = 0.005;
const float globalDensity            = 0.0025;
const float heightFalloff            = 0.0001;

// based on: https://developer.amd.com/wordpress/media/2012/10/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf
float volumetricFog(in vec3 pos, in vec3 cameraToWorldPos) {
  // float fogHeightDensityAtViewer = exp(-heightFalloff * pos.z);
  float fogInt = length(cameraToWorldPos) * fogHeightDensityAtViewer;
  if(abs(cameraToWorldPos.y) > 0.01) {
    float t = heightFalloff*cameraToWorldPos.y;
    fogInt *= (1.0-exp(-t))/t;
    }
  return 1.0-exp(-globalDensity*fogInt);
  }

#if !defined(FOG)
vec4 clounds(vec2 texc){
  vec4 cloudDL1 = texture(textureDayL1,texc*0.3+ubo.dxy1);
  vec4 cloudDL0 = texture(textureDayL0,texc*0.3+ubo.dxy0);
#ifdef G1
  return mix(cloudDL0,cloudDL1,cloudDL1.a);
#else
  return (cloudDL0+cloudDL1);
#endif
  }

vec4 stars(vec2 texc){
  vec4 cloudNL1 = texture(textureNightL1,texc*0.3+ubo.dxy1);
  vec4 cloudNL0 = texture(textureNightL0,texc*0.6);
#ifdef G1
  vec4 night    = mix(cloudNL0,cloudNL1,cloudNL1.a);
#else
  vec4 night    = cloudNL0+cloudNL0;
#endif
  return vec4(night.rgb,ubo.night);
  }
#endif

vec3 inverse(vec3 pos) {
  vec4 ret = ubo.mvpInv*vec4(pos,1.0);
  return ret.xyz/ret.w;
  }

void main() {
  vec3  view     = normalize(inverse(vec3(inPos,1.0)));
  vec3  sunDir   = ubo.sunDir;
  vec3  pos      = vec3(0,RPlanet+ubo.plPosY,0);

#if defined(FOG)
  vec2  uv       = inPos*0.5+vec2(0.5);
  float z        = texture(depth,uv).r;
  vec3 pos1      = inverse(vec3(inPos,z));
  vec3 pos0      = inverse(vec3(inPos,0));

  float dist     = length(pos1-pos0);
  vec3  mie      = fogMie(pos,view,sunDir,dist);
  float fogDens  = volumetricFog(pos0.xyz,pos1.xyz-pos0.xyz);
  vec3  fogColor = skyColor*fogDens;
  outColor       = vec4(mie+fogColor,fogDens);
#else
  vec3  color    = atmosphere(pos,view,sunDir);
  // Sun
  float alpha    = dot(view,sunDir);
  float spot     = smoothstep(0.0, 1000.0, phase(alpha, 0.9995));
  color += vec3(spot*1000.0);

  // Apply exposure.
  color          = exposure(color);

  float L        = rayIntersect(pos, view, RAtmos);
  vec3  cloudsAt = normalize(pos + view * L);
  vec2  texc     = vec2(cloudsAt.zx)*200.f;
  vec4  day      = clounds(texc);
  vec4  night    = stars(texc);
  vec4  cloud    = mix(day,night,ubo.night);

  color          = mix(color.rgb,cloud.rgb,min(1.0,cloud.a));
  outColor       = vec4(color,1.0);
#endif
  }
