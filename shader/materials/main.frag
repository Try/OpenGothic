#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define FRAGMENT
#include "materials_common.glsl"

layout(location = 0) in Varyings shInp;

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) in flat uint debugId;
#endif

#if !defined(SHADOW_MAP)
layout(location = 0) out vec4 outColor;
#endif

#if defined(GBUFFER)
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outDepth;
#endif

#if !defined(SHADOW_MAP)
vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z, float bias) {
  z += (bias/65535.0);
  z  = max(0,z);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float calcShadow(vec3 shPos0, vec3 shPos1) {
  vec4  lay0 = shadowSample(textureSm0,shPos0.xy);
  vec4  lay1 = shadowSample(textureSm1,shPos1.xy);

  float v0   = shadowResolve(lay0,shPos0.z,4.0);
  float v1   = shadowResolve(lay1,shPos1.z,1.0);

  if(abs(shPos0.x)<1.0 && abs(shPos0.y)<1.0 && ((0.45<lay1.x && lay1.x<0.55) || lay1.x==0))
    return v0;
  if(abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0)
    return v1;
  return 1.0;
  }

float calcShadow() {
  vec3 shPos0  = (shInp.shadowPos[0].xyz)/shInp.shadowPos[0].w;
  vec3 shPos1  = (shInp.shadowPos[1].xyz)/shInp.shadowPos[1].w;
  return calcShadow(shPos0,shPos1);
  }
#endif

#if !defined(SHADOW_MAP)
vec4 dbgLambert() {
  vec3  normal  = normalize(shInp.normal);
  float lambert = max(0.0,dot(scene.ldir,normal));
  return vec4(lambert,lambert,lambert,1.0);
  }

vec3 flatNormal(){
  vec3 pos = shInp.pos;
  vec3 dx  = dFdx(pos);
  vec3 dy  = dFdy(pos);
  return /*normalize*/(cross(dx,dy));
  }

vec3 calcLight() {
  vec3  normal  = normalize(shInp.normal);
  float lambert = max(0.0,dot(scene.ldir,normal));

#if (MESH_TYPE==T_LANDSCAPE)
  // fix self-shadowed surface
  float flatSh = dot(scene.ldir,flatNormal());
  if(flatSh<=0) {
    lambert = 0;
    }
#endif

  float light   = lambert*calcShadow();
  vec3  color   = scene.sunCl.rgb*clamp(light,0.0,1.0);
  return color + scene.ambient;
  }
#endif

#if defined(WATER)
vec3 waterColor(vec3 selfColor) {
  vec3  scr    = shInp.scr.xyz/shInp.scr.w;
  vec2  p      = scr.xy*0.5+vec2(0.5);
  vec4  back   = texture(gbufferDiffuse,p);
  float depth  = texture(gbufferDepth,  p).r;

  float ground   = reconstructCSZ(depth, scene.clipInfo);
  float water    = reconstructCSZ(scr.z, scene.clipInfo);
  /*
  float ratio   = 1.00 / 1.52; // air / water
  vec3  view    = normalize(shInp.pos - scene.camPos);
  vec3  refract = refract(view, shInp.normal, ratio);
  */

  float dist     = abs(water-ground)*10.0;
  float a        = min(1.0 - exp(-0.95 * dist), 0.95);

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

#if !defined(SHADOW_MAP)
  vec3  color = vec3(0);
  float alpha = 1;

#if defined(GHOST)
  color = ghostColor(t.rgb);
  alpha = t.a;
#else
  color = t.rgb;
  alpha = t.a;
#endif

#if defined(MAT_COLOR)
  color *= shInp.color.rgb;
  alpha *= shInp.color.a;
#endif

#if defined(LIGHT)
  color *= calcLight();
#endif

#if defined(WATER)
  {
    float f     = 0.02;
    vec3  view  = normalize(shInp.pos - scene.camPos);
    float rs    = cookTorrance(f, shInp.normal,scene.ldir,view, 0.01);

    vec4  wclr  = vec4(waterColor(color) + vec3(rs),1);

    //wclr   = vec4(rs,rs,rs,1);
    color  = wclr.rgb;
    alpha  = wclr.a;
  }
#endif

#if !defined(SHADOW_MAP)
  outColor      = vec4(color,alpha);
#endif

#ifdef GBUFFER
  outDiffuse    = t;
  outNormal     = vec4(normalize(shInp.normal)*0.5 + vec3(0.5),1.0);
  outDepth      = vec4(shInp.scr.z/shInp.scr.w,0.0,0.0,0.0);
#endif

#if DEBUG_DRAW
  outColor = vec4(debugColors[debugId%MAX_DEBUG_COLORS],1.0);
#endif

  //outColor = vec4(inZ.xyz/inZ.w,1.0);
  //outColor = vec4(vec3(inPos.xyz)/1000.0,1.0);
  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(calcLight()),1.0);
  //outColor = vec4(vec3(calcShadow()),1.0);
  //vec3 shPos0  = (shInp.shadowPos[0].xyz)/shInp.shadowPos[0].w;
  //outColor   = vec4(vec3(shPos0.xy,0),1.0);
  //outColor = dbgLambert();

#endif
  }
