#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform sampler2D textureD;
layout(binding = 3) uniform sampler2D textureSm;

#ifdef SHADOW_MAP
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
#else
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inLight;
layout(location = 5) in vec3 inAmbient;
layout(location = 6) in vec3 inSun;
#endif

layout(location = 0) out vec4 outColor;

const float shadowSize=2048.0;

float implShadowVal(in vec2 uv, in float shPosZ, in int layer) {
  float shMap = texture(textureSm,uv)[layer];
  float shZ   = min(0.99,shPosZ);

  return step(shZ-0.001,shMap);
  }

float shadowVal(in vec2 uv, in float shPosZ, in int layer) {
  // TODO: uniform shadowmap resolution
  vec2 offset = fract(uv.xy * shadowSize * 0.5);  // mod
  offset = vec2(offset.x>0.25,offset.y>0.25);
  // y ^= x in floating point
  offset.y += offset.x;
  if(offset.y>1.1)
    offset.y = 0;
  offset/=shadowSize;

  float d1 = 0.5/shadowSize;
  float d2 = 1.5/shadowSize;
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

void main() {
  vec4 t = texture(textureD,inUV);

#ifdef ATEST
  if(t.a<0.5)
    discard;
#endif

#ifdef SHADOW_MAP
  outColor = vec4(inShadowPos.zzz,0.0);
#else
  float lambert = max(0.0,dot(inLight,normalize(inNormal)));
  vec3  shPos0  = (inShadowPos.xyz)/inShadowPos.w;
  vec3  shPos1  = shPos0*0.2;

  float light = lambert;
#ifndef PFX
  light *= calcShadow(shPos0,shPos1);

  // vec3  ambient = vec3(0.25);//*inColor.xyz
  // vec3  diffuse = vec3(1.0)*inColor.xyz;
  // vec3  color   = mix(ambient,diffuse,clamp(light,0.0,1.0));

  vec3  color   = (inAmbient+inSun*inColor.xyz*clamp(light,0.0,1.0));
#else
  vec3  color   = t.rgb*inColor.rgb;
#endif
  outColor      = vec4(t.rgb*color,t.a);

  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(light),1.0);
  //vec4 shMap = texture(textureSm,shPos1.xy*vec2(0.5,0.5)+vec2(0.5));
  //outColor   = vec4(vec3(shMap.ggg),1.0);
#endif
  }
