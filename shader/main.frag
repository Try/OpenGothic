#version 450
#extension GL_ARB_separate_shader_objects : enable

#define LIGHT_CNT 6

layout(binding = 0) uniform sampler2D textureD;
layout(binding = 1) uniform sampler2D textureSm;

#ifdef SHADOW_MAP
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
#else
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inPos;
#endif

layout(location = 0) out vec4 outColor;

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

layout(std140,binding = 2) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  mv;
  mat4  shadow;
  vec3  ambient;
  vec4  sunCl;
  } scene;

#if defined(OBJ)
layout(std140,binding = 3) uniform UboObject {
  mat4  obj;
  Light light[LIGHT_CNT];
  } ubo;
#endif

float implShadowVal(in vec2 uv, in float shPosZ, in int layer) {
  float shMap = texture(textureSm,uv)[layer];
  float shZ   = min(0.99,shPosZ);

  return step(shZ-0.001,shMap);
  }

float shadowVal(in vec2 uv, in float shPosZ, in int layer) {
  // TODO: uniform shadowmap resolution
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

#if !defined(SHADOW_MAP)
float calcShadow() {
  vec3 shPos0  = (inShadowPos.xyz)/inShadowPos.w;
  vec3 shPos1  = shPos0*0.2;
  return calcShadow(shPos0,shPos1);
  }
#endif

#if !defined(SHADOW_MAP)
vec3 calcLight() {
  vec3  normal  = normalize(inNormal);
  float lambert = max(0.0,dot(scene.ldir,normal));
  float light   = lambert*calcShadow();
  vec3  color   = scene.sunCl.rgb*clamp(light,0.0,1.0);

#if defined(OBJ)
  for(int i=0; i<LIGHT_CNT; ++i) {
    vec3  ldir    = ubo.light[i].pos.xyz - inPos.xyz;
    float rgn     = ubo.light[i].range;
    float qDist   = dot(ldir,ldir);
    float lambert = max(0.0,dot(normalize(ldir),normal));

    // return vec3(lambert);
    // return vec3(length(ldir/rgn));

    float light = (1.0-length(ldir/rgn))*lambert;

    color += ubo.light[i].color * clamp(light,0.0,1.0);
    }
#endif

  return inColor.rgb * color;
  }
#endif

void main() {
  vec4 t = texture(textureD,inUV);

#ifdef ATEST
  if(t.a<0.5)
    discard;
#endif

#ifdef SHADOW_MAP
  outColor = vec4(inShadowPos.zzz,0.0);
#else

#ifndef PFX
  vec3  color   = scene.ambient + calcLight();
#else
  vec3  color   = inColor.rgb;
#endif
  outColor      = vec4(t.rgb*color,t.a*inColor.a);

  //outColor = vec4(vec3(inPos.xyz)/1000.0,1.0);
  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(calcLight()),1.0);
  //vec4 shMap = texture(textureSm,shPos1.xy*vec2(0.5,0.5)+vec2(0.5));
  //outColor   = vec4(vec3(shMap.ggg),1.0);
#endif
  }
