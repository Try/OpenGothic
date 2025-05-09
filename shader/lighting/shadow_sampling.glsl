#ifndef SHADOWSAMPLING_GLSL
#define SHADOWSAMPLING_GLSL

#include "scene.glsl"

vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

vec4 shadowSample(in sampler2D shadowMap, vec2 shPos, out vec2 m) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  m        = fract(shPos.xy * textureSize(shadowMap, 0) - vec2(0.5));
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z) {
  z  = max(0,z);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float shadowResolve(in vec4 sh, float z, vec2 m) {
  const float bias = 0.0002;
  z  = max(0, z);
  sh = step(sh, vec4(z));

  vec2 xx = mix(sh.wz, sh.xy, m.y);
  return    mix(xx.x,  xx.y,  m.x);
  }

float calcShadowMs4(in sampler2D shadowMap0, vec3 shPos0) {
  vec2  bias = vec2(1)/vec2(textureSize(shadowMap0, 0));
  vec4  lay0 = vec4(0);
  float ret  = 0;

  vec2  m0;
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2(-1.5,  0.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2( 0.5,  0.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2(-1.5, -1.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);
  lay0 = shadowSample(shadowMap0,shPos0.xy + bias*vec2( 0.5, -1.5), m0);
  ret += shadowResolve(lay0,shPos0.z,m0);

  return ret * 0.25;
  }

float calcShadow(in SceneDesc scene,
                 in sampler2D shadowMap0, vec3 shPos0,
                 in sampler2D shadowMap1, vec3 shPos1) {
  vec2 minMax = scene.closeupShadowSlice;
  vec2 m1;
  vec4 lay1   = shadowSample(shadowMap1,shPos1.xy, m1);
  lay1.x = max(max(lay1.x,lay1.y), max(lay1.z,lay1.w));

  if(abs(shPos0.x)<0.99 && abs(shPos0.y)<0.99 && lay1.x<minMax[1]) {
    return calcShadowMs4(shadowMap0,shPos0);
    }

  if(abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0) {
    return calcShadowMs4(shadowMap1,shPos1);
    }
  return 1.0;
  }

float calcShadow(in vec3 pos, const vec3 normal, in SceneDesc scene, in sampler2D shadowMap0, in sampler2D shadowMap1) {
  vec4 shadowPos[2];
#if defined(LWC)
  shadowPos[0] = scene.viewShadowLwc[0]*vec4(pos + normal*5,  1.0);
  shadowPos[1] = scene.viewShadowLwc[1]*vec4(pos + normal*25, 1.0);
#else
  shadowPos[0] = scene.viewShadow   [0]*vec4(pos + normal*5, 1.0);
  shadowPos[1] = scene.viewShadow   [1]*vec4(pos + normal*25, 1.0);
#endif

  vec3 shPos0  = (shadowPos[0].xyz)/shadowPos[0].w;
  vec3 shPos1  = (shadowPos[1].xyz)/shadowPos[1].w;
  return calcShadow(scene, shadowMap0,shPos0, shadowMap1,shPos1);
  }

float calcShadow(in vec4 pos4, in float bias, in SceneDesc scene, in sampler2D shadowMap0, in sampler2D shadowMap1) {
  vec4 shadowPos[2];
  vec3 offset = bias*scene.sunDir*pos4.w;
#if defined(LWC)
  shadowPos[0] = scene.viewShadowLwc[0]*vec4(pos4.xyz + offset*1.0, pos4.w);
  shadowPos[1] = scene.viewShadowLwc[1]*vec4(pos4.xyz + offset*3.0, pos4.w);
#else
  shadowPos[0] = scene.viewShadow   [0]*vec4(pos4.xyz + offset*1.0, pos4.w);
  shadowPos[1] = scene.viewShadow   [1]*vec4(pos4.xyz + offset*3.0, pos4.w);
#endif

  vec3 shPos0  = (shadowPos[0].xyz)/shadowPos[0].w;
  vec3 shPos1  = (shadowPos[1].xyz)/shadowPos[1].w;
  return calcShadow(scene, shadowMap0,shPos0, shadowMap1,shPos1);
  }

#endif
