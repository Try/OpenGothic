#ifndef SHADOWSAMPLING_GLSL
#define SHADOWSAMPLING_GLSL

#include "../scene.glsl"

vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z) {
  z  = max(0,z);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float calcShadow(in SceneDesc scene,
                 in sampler2D shadowMap0, vec3 shPos0,
                 in sampler2D shadowMap1, vec3 shPos1) {
  vec4  lay0   = shadowSample(shadowMap0,shPos0.xy);
  vec4  lay1   = shadowSample(shadowMap1,shPos1.xy);

  vec2  minMax = scene.closeupShadowSlice;
  bool  inSm0  = abs(shPos0.x)<1.0 && abs(shPos0.y)<1.0;
  bool  inSm1  = abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0;

  if(inSm0 && lay1.x<minMax[1])
    return shadowResolve(lay0,shPos0.z);
  if(inSm1)
    return shadowResolve(lay1,shPos1.z);
  return 1.0;
  }

float calcShadow(in vec4 pos4, in SceneDesc scene, in sampler2D shadowMap0, in sampler2D shadowMap1) {
  vec4 shadowPos[2];
  shadowPos[0] = scene.viewShadow[0]*vec4(pos4);
  shadowPos[1] = scene.viewShadow[1]*vec4(pos4);

  vec3 shPos0  = (shadowPos[0].xyz)/shadowPos[0].w;
  vec3 shPos1  = (shadowPos[1].xyz)/shadowPos[1].w;
  return calcShadow(scene, shadowMap0,shPos0, shadowMap1,shPos1);
  }

#endif
