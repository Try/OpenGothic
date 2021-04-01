#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define FRAGMENT
#include "shader_common.glsl"

layout(location = 0) in VsData {
#if defined(SHADOW_MAP)
  vec2 uv;
  vec4 scr;
#else
  vec2 uv;
  vec4 shadowPos[2];
  vec3 normal;
  vec4 color;
  vec4 pos;
  vec4 scr;
#endif
  } shInp;

#ifdef GBUFFER
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outDepth;
#else
layout(location = 0) out vec4 outColor;
#endif

#if !defined(SHADOW_MAP)
vec4 shadowSample(in sampler2D shadowMap,vec3 shPos) {
  shPos.z = max(0.0,shPos.z);
  vec4 sh = textureGather(shadowMap,shPos.xy);
  return step(sh,vec4(shPos.z));
  }

float shadowResolve(in sampler2D shadowMap,vec3 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  shPos.z -= (1.f/32768.0);
  vec4  sh = shadowSample(shadowMap,shPos);
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  /*
  vec2  k  = fract(textureSize(shadowMap,0)*shPos.xy);

  float y0 = mix(sh.x,sh.y, k.x);
  float y1 = mix(sh.w,sh.z, k.x);

  return mix(y0,y1, k.y);
  */
  }

float implShadowVal(in sampler2D shadowMap, in vec2 uv, in float shPosZ, in float shBias) {
  float shMap = texture(shadowMap,uv).r;
  float shZ   = min(0.99,shPosZ);

  return step(shMap,shZ-shBias);
  }

float shadowVal(in sampler2D shadowMap, in vec2 uv, in float shPosZ, in float shBias) {
  vec2 offset = fract(uv.xy * scene.shadowSize * 0.5);  // mod
  offset = vec2(offset.x>0.25,offset.y>0.25);
  // y ^= x in floating point
  offset.y += offset.x;
  if(offset.y>1.1)
    offset.y = 0;
  offset/=scene.shadowSize;

  float d1 = 0.5/scene.shadowSize;
  float d2 = 1.5/scene.shadowSize;
  float ret = implShadowVal(shadowMap,uv+offset+vec2(-d2, d1),shPosZ,shBias) +
              implShadowVal(shadowMap,uv+offset+vec2( d1, d1),shPosZ,shBias) +
              implShadowVal(shadowMap,uv+offset+vec2(-d2, d2),shPosZ,shBias) +
              implShadowVal(shadowMap,uv+offset+vec2( d1,-d2),shPosZ,shBias);
  return ret*0.25;
  }

float calcShadow(vec3 shPos0, vec3 shPos1) {
  float lay0 = shadowResolve(textureSm0,shPos0);
  float lay1 = shadowResolve(textureSm1,shPos1);

  if(abs(shPos0.x)<0.99 && abs(shPos0.y)<0.99)
    return lay0;
  if(abs(shPos1.x)<0.99 && abs(shPos1.y)<0.99)
    return lay1;
  return 1.0;
  }

float calcShadow() {
  vec3 shPos0  = (shInp.shadowPos[0].xyz)/shInp.shadowPos[0].w;
  vec3 shPos1  = (shInp.shadowPos[1].xyz)/shInp.shadowPos[1].w;
  return calcShadow(shPos0,shPos1);
  }
#endif

#if !defined(SHADOW_MAP)
vec3 calcLight() {
  vec3  normal  = normalize(shInp.normal);
  float lambert = max(0.0,dot(scene.ldir,normal));
  float light   = lambert*calcShadow();
  vec3  color   = shInp.color.rgb*scene.sunCl.rgb*clamp(light,0.0,1.0);

  /*
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
    }*/

  return color + scene.ambient;
  }
#endif

#if defined(WATER)
vec3 waterColor(vec3 selfColor) {
  vec3  scr   = shInp.scr.xyz/shInp.scr.w;
  vec2  p     = scr.xy*0.5+vec2(0.5);
  vec4  back  = texture(gbufferDiffuse,p);
  float depth = texture(gbufferDepth,p).r;

  vec4 ground = scene.modelViewInv*vec4(scr.xy,depth,1.0);
  vec4 water  = scene.modelViewInv*vec4(scr,1.0);

  float dist  = distance(water.xyz/water.w,ground.xyz/ground.w)/100.0;
  dist = pow(dist,2);
  float a     = min(dist,0.8);
  return mix(back.rgb,selfColor,a);
  }
#endif

#if defined(GHOST)
vec3 ghostColor(vec3 selfColor) {
  vec3  scr   = shInp.scr.xyz/shInp.scr.w;
  vec2  p     = scr.xy*0.5+vec2(0.5);
  vec4  back  = texture(gbufferDiffuse,p);
  return back.rgb+selfColor;
  }
#endif

void main() {
#if !defined(SHADOW_MAP) || defined(ATEST)
  vec4 t = texture(textureD,shInp.uv);
#  ifdef ATEST
  if(t.a<0.5)
    discard;
#  endif
#endif

#ifdef SHADOW_MAP
  outColor = vec4(shInp.scr.zzz,0.0);
#else

#if defined(EMMISSIVE)
  vec3 color = shInp.color.rgb;
#else
  vec3 color = calcLight();
#endif

  color = t.rgb*color;
#if defined(WATER)
  color = waterColor(color);
#endif

#if defined(GHOST)
  color = ghostColor(color);
#endif

  outColor      = vec4(color,t.a*shInp.color.a);

#ifdef GBUFFER
  outDiffuse    = t;
  outNormal     = vec4(normalize(shInp.normal)*0.5 + vec3(0.5),1.0);
  outDepth      = vec4(shInp.scr.z/shInp.scr.w,0.0,0.0,0.0);
#endif

  //outColor   = vec4(inZ.xyz/inZ.w,1.0);
  //outColor = vec4(vec3(inPos.xyz)/1000.0,1.0);
  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(calcLight()),1.0);
  //vec3 shPos0  = (shInp.shadowPos[0].xyz)/shInp.shadowPos[0].w;
  //outColor   = vec4(vec3(shPos0.zzz),1.0);
#endif
  }
