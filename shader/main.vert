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
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec3 inPos0;
layout(location = 4) in vec3 inPos1;
layout(location = 5) in vec3 inPos2;
layout(location = 6) in vec3 inPos3;
layout(location = 7) in vec4 inId;
layout(location = 8) in vec4 inWeight;
#else
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
#endif

#ifdef SHADOW_MAP
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outShadowPos;
#else
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outShadowPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec4 outPos;
layout(location = 5) out vec4 outScr;
#endif

vec4 vertexPos() {
#ifdef SKINING
  vec4 pos0 = vec4(inPos0,1.0);
  vec4 pos1 = vec4(inPos1,1.0);
  vec4 pos2 = vec4(inPos2,1.0);
  vec4 pos3 = vec4(inPos3,1.0);
  vec4 t0   = anim.skel[int(inId.x*255.0)]*pos0;
  vec4 t1   = anim.skel[int(inId.y*255.0)]*pos1;
  vec4 t2   = anim.skel[int(inId.z*255.0)]*pos2;
  vec4 t3   = anim.skel[int(inId.w*255.0)]*pos3;
  return t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
#else
  return vec4(inPos,1.0);
#endif
  }

vec4 normal(){
#ifdef SKINING
  //vec4 norm = vec4(inNormal.z,inNormal.y,inNormal.x,0.0);
  vec4 norm = vec4(inNormal,0.0);
  vec4 n0   = anim.skel[int(inId.x)]*norm;
  vec4 n1   = anim.skel[int(inId.y)]*norm;
  vec4 n2   = anim.skel[int(inId.z)]*norm;
  vec4 n3   = anim.skel[int(inId.w)]*norm;
  vec4 n    = (n0*inWeight.x + n1*inWeight.y + n2*inWeight.z + n3*inWeight.w);
  return vec4(n.xyz,0.0);
#endif
#ifdef OBJ
  return vec4(inNormal.x,inNormal.y,inNormal.z,0.0);
#endif
  return vec4(-inNormal.z,inNormal.y,inNormal.x,0.0);
  }

void main() {
#if defined(OBJ)
  outUV      = inUV + material.texAnim;
#else
  outUV      = inUV;
#endif

  vec4 pos   = vertexPos();
#ifdef OBJ
  vec4 shPos = scene.shadow*push.obj*pos;
#else
  vec4 shPos = scene.shadow*pos;
#endif

#ifdef SHADOW_MAP
  outShadowPos = shPos;
  gl_Position  = shPos;
#else
  vec4 norm = normal();
  outShadowPos = shPos;
  outColor     = inColor;
#  ifdef OBJ
  outNormal    = (push.obj*norm).xyz;
  outPos       = (push.obj*pos);
  vec4 trPos   = scene.mv*outPos;
  outScr       = trPos;
  gl_Position  = trPos;
#  else
  outNormal    = norm.xyz;
  outPos       = pos;

  vec4 trPos   = scene.mv*pos;
  outScr       = trPos;
  gl_Position  = trPos;
#  endif
#endif
  }
