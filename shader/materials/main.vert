#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define VERTEX
#include "materials_common.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

#if (MESH_TYPE==T_SKINING)
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inColor;
layout(location = 3) in vec3 inPos0;
layout(location = 4) in vec3 inPos1;
layout(location = 5) in vec3 inPos2;
layout(location = 6) in vec3 inPos3;
layout(location = 7) in uint inId;
layout(location = 8) in vec4 inWeight;
#else
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inColor;
#endif

layout(location = 0) out Varyings shOut;

uint  objId  = 0;
uvec4 boneId = uvec4(0);

#if (MESH_TYPE==T_MORPH)
vec3 morphOffset(int i) {
  vec2  ai        = unpackUnorm2x16(push.morph[i].alpha16_intensity16);
  float alpha     = ai.x;
  float intensity = ai.y;
  if(intensity<=0)
    return vec3(0);

  uint  vId   = gl_VertexIndex + push.morph[i].indexOffset;
  int   index = morphId.index[vId];
  if(index<0)
    return vec3(0);

  uint  f0 = push.morph[i].sample0;
  uint  f1 = push.morph[i].sample1;
  vec3  a  = morph.samples[f0 + index].xyz;
  vec3  b  = morph.samples[f1 + index].xyz;

  return mix(a,b,alpha) * intensity;
  }
#endif

vec3 vertexNormal() {
#if (MESH_TYPE==T_SKINING)
  vec4 n = anim.skel[objId]*vec4(inNormal,0.0);
  return vec3(n.z,n.y,-n.x);
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_MORPH)
  vec4 n = anim.skel[objId]*vec4(inNormal,0.0);
  return vec3(n.xyz);
#else
  return inNormal;
#endif
  }

vec3 vertexPos() {
  vec3 dpos = vec3(0);
#if MESH_TYPE==T_MORPH
  for(int i=0; i<MAX_MORPH_LAYERS; ++i)
    dpos += morphOffset(i);
#endif

#if defined(LVL_OBJECT)
  dpos += inNormal*push.fatness;
#endif

#if (MESH_TYPE==T_SKINING)
  vec4 pos0 = vec4(inPos0+dpos,1.0);
  vec4 pos1 = vec4(inPos1+dpos,1.0);
  vec4 pos2 = vec4(inPos2+dpos,1.0);
  vec4 pos3 = vec4(inPos3+dpos,1.0);
  vec4 t0   = anim.skel[boneId.x]*pos0;
  vec4 t1   = anim.skel[boneId.y]*pos1;
  vec4 t2   = anim.skel[boneId.z]*pos2;
  vec4 t3   = anim.skel[boneId.w]*pos3;
  vec4 pos  =  t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
  return pos.xyz;
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_MORPH)
  vec4 pos  = anim.skel[boneId.x]*vec4(inPos+dpos,1.0);
  return pos.xyz;
#else
  return inPos;
#endif
  }

vec2 texcoord() {
#if defined(MAT_ANIM)
  return inUV + material.texAnim;
#else
  return inUV;
#endif
  }

void main() {
#if (MESH_TYPE==T_SKINING)
  boneId   = uvec4(unpackUnorm4x8(inId)*255.0);
#endif
#if defined(LVL_OBJECT)
  objId   = push.matrixId;
  boneId  += uvec4(objId);
#endif

  vec3 pos = vertexPos();

  shOut.uv = texcoord();

#if !defined(SHADOW_MAP)
  shOut.shadowPos[0] = scene.shadow[0]*vec4(pos,1.0);
  shOut.shadowPos[1] = scene.shadow[1]*vec4(pos,1.0);
  shOut.normal       = vertexNormal();
#endif

#if !defined(SHADOW_MAP) || defined(WATER)
  shOut.pos   = pos;
#endif

#if !defined(SHADOW_MAP) && (MESH_TYPE==T_PFX)
  shOut.color = unpackUnorm4x8(inColor);
#endif

  vec4 trPos  = scene.viewProject*vec4(pos,1.0);
  shOut.scr   = trPos;
  gl_Position = trPos;
  }
