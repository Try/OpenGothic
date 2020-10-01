#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define FRAGMENT
#include "shader_common.glsl"

#ifdef SHADOW_MAP
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
#else
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inPos;
layout(location = 5) in vec4 inScr;
#endif

#ifdef GBUFFER
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outDepth;
#else
layout(location = 0) out vec4 outColor;
#endif

#if !defined(SHADOW_MAP)
float implShadowVal(in vec2 uv, in float shPosZ, in int layer) {
  float shMap = texture(textureSm,uv)[layer];
  float shZ   = min(0.99,shPosZ);

  return step(shZ-0.001,shMap);
  }

float shadowVal(in vec2 uv, in float shPosZ, in int layer) {
  vec2 offset = fract(uv.xy * scene.shadowSize * 0.5);  // mod
  offset = vec2(offset.x>0.25,offset.y>0.25);
  // y ^= x in floating point
  offset.y += offset.x;
  if(offset.y>1.1)
    offset.y = 0;
  offset/=scene.shadowSize;

  float d1 = 0.5/scene.shadowSize;
  float d2 = 1.5/scene.shadowSize;
  float ret = implShadowVal(uv+offset+vec2(-d2, d1),shPosZ,layer) +
              implShadowVal(uv+offset+vec2( d1, d1),shPosZ,layer) +
              implShadowVal(uv+offset+vec2(-d2, d2),shPosZ,layer) +
              implShadowVal(uv+offset+vec2( d1,-d2),shPosZ,layer);

  return ret*0.25;
  }

float calcShadow(vec3 shPos0, vec3 shPos1) {
  if(abs(shPos0.x)<0.99 && abs(shPos0.y)<0.99)
    return shadowVal(shPos0.xy*vec2(0.5,0.5)+vec2(0.5),shPos0.z,0);

  if(abs(shPos1.x)<0.99 && abs(shPos1.y)<0.99)
    return implShadowVal(shPos1.xy*vec2(0.5,0.5)+vec2(0.5),shPos1.z,1);

  return 1.0;
  }

float calcShadow() {
  vec3 shPos0  = (inShadowPos.xyz)/inShadowPos.w;
  vec3 shPos1  = shPos0*0.2;
  return calcShadow(shPos0,shPos1);
  }
#endif

#if !defined(SHADOW_MAP)
vec3 calcLight() {
  vec3  normal  = normalize(inNormal);
#if defined(LIGHT_EXT)
  vec3  color   = vec3(0.0);
#else
  float lambert = max(0.0,dot(scene.ldir,normal));
  float light   = lambert*calcShadow();
  vec3  color   = inColor.rgb*scene.sunCl.rgb*clamp(light,0.0,1.0);
#endif

#if defined(OBJ)
  for(int i=0; i<LIGHT_BLOCK; ++i) {
    float rgn     = push.light[i].range;
    if(rgn<=0.0)
      continue;
    vec3  ldir    = push.light[i].pos.xyz - inPos.xyz;
    float qDist   = dot(ldir,ldir);
    float lambert = max(0.0,dot(normalize(ldir),normal));

    // return vec3(lambert);
    // return vec3(length(ldir/rgn));

    float light = (1.0-(qDist/(rgn*rgn)))*lambert;

    color += push.light[i].color * clamp(light,0.0,1.0);
    }
#endif

#if defined(LIGHT_EXT)
  return color;
#else
  return color + scene.ambient;
#endif
  }
#endif

#if defined(WATER)
vec3 waterColor(vec3 selfColor) {
  vec3  scr   = inScr.xyz/inScr.w;
  vec2  p     = scr.xy*0.5+vec2(0.5);
  vec4  back  = texture(gbufferDiffuse,p);
  float depth = texture(gbufferDepth,p).r;

  vec4 ground = scene.modelViewInv*vec4(scr.xy,depth,1.0);
  vec4 water  = scene.modelViewInv*vec4(scr,1.0);

  float dist  = 0.001*distance(water.xyz/water.w,ground.xyz/ground.w);
  float a     = min(dist,0.98);
  return mix(back.rgb,selfColor,a);
  }
#endif

void main() {
#if !defined(SHADOW_MAP) || defined(ATEST)
  vec4 t = texture(textureD,inUV);
#  ifdef ATEST
  if(t.a<0.5)
    discard;
#  endif
#endif

#ifdef SHADOW_MAP
  outColor = vec4(inShadowPos.zzz,0.0);
#else

#if defined(EMMISSIVE)
  vec3 color = inColor.rgb;
#else
  vec3 color = calcLight();
#endif

  color = t.rgb*color;
#if defined(WATER)
  color = waterColor(color);
#endif

  outColor      = vec4(color,t.a*inColor.a);

#ifdef GBUFFER
  outDiffuse    = t;
  outNormal     = vec4(normalize(inNormal)*0.5 + vec3(0.5),1.0);
  outDepth      = vec4(inScr.z/inScr.w,0.0,0.0,0.0);
#endif

  //outColor   = vec4(inZ.xyz/inZ.w,1.0);
  //outColor = vec4(vec3(inPos.xyz)/1000.0,1.0);
  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(calcLight()),1.0);
  //vec4 shMap = texture(textureSm,shPos1.xy*vec2(0.5,0.5)+vec2(0.5));
  //outColor   = vec4(vec3(shMap.ggg),1.0);
#endif
  }
