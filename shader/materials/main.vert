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

#if (MESH_TYPE==T_SKINING)
uvec4 boneId;
#endif

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

vec4 vertexPosMesh() {
#if (MESH_TYPE==T_SKINING)
  vec4 pos0 = vec4(inPos0,1.0);
  vec4 pos1 = vec4(inPos1,1.0);
  vec4 pos2 = vec4(inPos2,1.0);
  vec4 pos3 = vec4(inPos3,1.0);
  vec4 t0   = anim.skel[boneId.x]*pos0;
  vec4 t1   = anim.skel[boneId.y]*pos1;
  vec4 t2   = anim.skel[boneId.z]*pos2;
  vec4 t3   = anim.skel[boneId.w]*pos3;
  return t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
#elif (MESH_TYPE==T_MORPH)
  vec3 v = inPos;
  for(int i=0; i<3; ++i)
    v += morphOffset(i);
  return vec4(v,1.0);
#else
  return vec4(inPos,1.0);
#endif
  }

vec4 vertexNormalMesh() {
#if (MESH_TYPE==T_SKINING)
  // vec4 norm = vec4(inNormal.xzy,0.0);
  // vec4 n0   = anim.skel[boneId.x]*norm;
  // vec4 n1   = anim.skel[boneId.y]*norm;
  // vec4 n2   = anim.skel[boneId.z]*norm;
  // vec4 n3   = anim.skel[boneId.w]*norm;
  // vec4 n    = (n0*inWeight.x + n1*inWeight.y + n2*inWeight.z + n3*inWeight.w);
  vec4 n = anim.skel[0]*vec4(inNormal,0.0);
  return vec4(n.z,n.y,-n.x,0.0);
#else
  return vec4(inNormal,0.0);
#endif
  }

vec3 vertexNormal() {
  vec4 norm = vertexNormalMesh();
#if defined(LVL_OBJECT)
  return (push.obj*norm).xyz;
#else
  return norm.xyz;
#endif
  }

vec4 vertexPos() {
  vec4 pos = vertexPosMesh();
#if defined(LVL_OBJECT)
  return push.obj*pos;
#else
  return pos;
#endif
  }

void main() {
#if (MESH_TYPE==T_SKINING)
  boneId   = uvec4(unpackUnorm4x8(inId)*255.0);
#endif

#if defined(MAT_ANIM)
  shOut.uv = inUV + material.texAnim;
#else
  shOut.uv = inUV;
#endif

#if defined(SHADOW_MAP)
  vec3 normal = vec3(0);
#else
  vec3 normal = vertexNormal();
#endif

  vec4 pos = vertexPos();
#if defined(LVL_OBJECT)
  vec3 fatOffset = vec3(0);
  if(push.fatness!=0) {
    if(normal==vec3(0))
      fatOffset = vertexNormal(); else
      fatOffset = normal;
    }
  pos.xyz += fatOffset;
#endif

#if !defined(SHADOW_MAP)
  shOut.shadowPos[0] = scene.shadow[0]*pos;
  shOut.shadowPos[1] = scene.shadow[1]*pos;
  shOut.normal       = normal;
#endif

#if !defined(SHADOW_MAP) || defined(WATER)
  shOut.pos   = pos.xyz;
#endif

#if !defined(SHADOW_MAP) && (MESH_TYPE==T_PFX)
  shOut.color = unpackUnorm4x8(inColor);
#endif

  vec4 trPos  = scene.viewProject*pos;
  shOut.scr   = trPos;
  gl_Position = trPos;
  }
