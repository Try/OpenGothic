#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define VERTEX
#include "shader_common.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

#ifdef SKINING
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

layout(location = 0) out VsData {
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
  } shOut;

#ifdef SKINING
vec4 boneId;
#endif

vec4 vertexPosMesh() {
#if defined(SKINING)
  vec4 pos0 = vec4(inPos0,1.0);
  vec4 pos1 = vec4(inPos1,1.0);
  vec4 pos2 = vec4(inPos2,1.0);
  vec4 pos3 = vec4(inPos3,1.0);
  vec4 t0   = anim.skel[int(boneId.x*255.0)]*pos0;
  vec4 t1   = anim.skel[int(boneId.y*255.0)]*pos1;
  vec4 t2   = anim.skel[int(boneId.z*255.0)]*pos2;
  vec4 t3   = anim.skel[int(boneId.w*255.0)]*pos3;
  return t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
#elif defined(MORPH)
  int index = morphId.index[gl_VertexIndex/4][gl_VertexIndex%4];
  if(index>=0) {
    int  f0 = push.morphFrame0*push.samplesPerFrame;
    int  f1 = push.morphFrame1*push.samplesPerFrame;
    vec4 a  = morph.samples[f0 + index];
    vec4 b  = morph.samples[f1 + index];

    vec4 displace = mix(a,b,push.morphAlpha);
    return vec4(inPos+displace.xyz,1.0);
    }
  return vec4(inPos,1.0);
#else
  return vec4(inPos,1.0);
#endif
  }

vec4 normalWorld() {
#if defined(SKINING)
  vec4 norm = vec4(inNormal,0.0);
  vec4 n0   = anim.skel[int(boneId.x)]*norm;
  vec4 n1   = anim.skel[int(boneId.y)]*norm;
  vec4 n2   = anim.skel[int(boneId.z)]*norm;
  vec4 n3   = anim.skel[int(boneId.w)]*norm;
  vec4 n    = (n0*inWeight.x + n1*inWeight.y + n2*inWeight.z + n3*inWeight.w);
  return vec4(n.z,n.y,-n.x,0.0);
#elif defined(OBJ)
  return vec4(inNormal,0.0);
#endif
  return vec4(inNormal,0.0);
  }

vec3 normal() {
  vec4 norm = normalWorld();
#if defined(OBJ)
  return (push.obj*norm).xyz;
#else
  return norm.xyz;
#endif
  }

vec4 vertexPos() {
  vec4 pos = vertexPosMesh();
#if defined(OBJ)
  return push.obj*pos;
#else
  return pos;
#endif
  }

void main() {
#if defined(SKINING)
  boneId = unpackUnorm4x8(inId);
#endif

#if !defined(SHADOW_MAP)
  shOut.color = unpackUnorm4x8(inColor);
#endif

#if defined(OBJ)
  shOut.uv = inUV + material.texAnim;
#else
  shOut.uv = inUV;
#endif

#if !defined(SHADOW_MAP)
  shOut.normal = normal();
#endif

  vec4 pos        = vertexPos();
  vec4 trPos      = scene.mv*pos;

#if !defined(SHADOW_MAP)
  shOut.shadowPos[0] = scene.shadow[0]*pos;
  shOut.shadowPos[1] = scene.shadow[1]*pos;
  shOut.pos          = pos;
#endif

  shOut.scr       = trPos;
  gl_Position     = trPos;
  }
