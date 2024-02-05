#ifndef MATERIALS_COMMON_GLSL
#define MATERIALS_COMMON_GLSL

#include "common.glsl"
#include "scene.glsl"

#if defined(BINDLESS)
#extension GL_EXT_nonuniform_qualifier : enable
#endif

/*
  GBUFFER | FORWARD | DEPTH_ONLY | WATER | EMISSIVE | GHOST
  VT_COLOR
  ATEST
  BINDLESS
*/

#define DEBUG_DRAW 0

#if DEBUG_DRAW
#define DEBUG_DRAW_LOC   20
#define MAX_DEBUG_COLORS 10
const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };
#endif

#define MAX_NUM_SKELETAL_NODES 96
#define MAX_MORPH_LAYERS       4
#define MaxVert                64
#define MaxPrim                64
#define MaxInd                 (MaxPrim*3)

#define T_LANDSCAPE 0
#define T_OBJ       1
#define T_SKINING   2
#define T_MORPH     3
#define T_PFX       4

const uint L_Scene    = 0;
const uint L_Payload  = 1;
const uint L_Instance = 2;
const uint L_Pfx      = L_Instance;
const uint L_Bucket   = 3;
const uint L_Ibo      = 4;
const uint L_Vbo      = 5;
const uint L_Diffuse  = 6;
const uint L_Sampler  = 7;
const uint L_Shadow0  = 8;
const uint L_Shadow1  = 9;
const uint L_MorphId  = 10;
const uint L_Morph    = 11;
const uint L_SceneClr = 12;
const uint L_GDepth   = 13;

#ifndef MESH_TYPE
#define MESH_TYPE 255
#endif

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING || MESH_TYPE==T_MORPH)
#define LVL_OBJECT 1
#endif

// Varying defines (derived)
#if !defined(DEPTH_ONLY) || defined(ATEST)
#define MAT_UV 1
#endif

#if !defined(DEPTH_ONLY)
#define MAT_NORMAL 1
#endif

#if defined(FLAT_NORMAL) || defined(FORWARD) || defined(WATER)
#define MAT_POSITION 1
#endif

#if defined(VT_COLOR) && !defined(DEPTH_ONLY)
#define MAT_COLOR 1
#endif

#if defined(MAT_UV) || defined(MAT_NORMAL) || defined(MAT_POSITION) || defined(MAT_COLOR)
#define MAT_VARYINGS 1
#endif

struct Varyings {
#if defined(MAT_UV)
  vec2 uv;
#endif

#if defined(MAT_NORMAL)
  vec3 normal;
#endif

#if defined(MAT_POSITION)
  vec3 pos; // NOTE: per-primitve normal and shadow projection. need to remove
#endif

#if defined(MAT_COLOR)
  vec4 color;
#endif

#if !defined(MAT_VARYINGS)
  float dummy; // GLSL has no support for empty structs
#endif
  };

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

struct MorphDesc {
  uint  indexOffset;
  uint  sample0;
  uint  sample1;
  uint  alpha16_intensity16;
  };

struct Instance {
  mat4x3 mat;
  float  fatness;
  uint   animPtr;
  uint   padd0;
  uint   padd1;
  };

struct IndirectCmd {
  uint vertexCount;
  uint instanceCount;
  uint firstVertex;
  uint firstInstance;
  uint writeOffset;
  };

struct Cluster {
  vec4  sphere;
  uint  bucketId_commandId;
  uint  firstMeshlet;
  int   meshletCount;
  uint  instanceId;
  };

struct Bucket {
  vec4  bbox[2];
  ivec2 texAniMapDirPeriod;
  float bboxRadius;
  float waveMaxAmplitude;
  float alphaWeight;
  float envMapping;
  };

layout(binding = L_Scene, std140) uniform UboScene {
  SceneDesc scene;
  };

#if !defined(CLUSTER) && (MESH_TYPE!=T_PFX)
layout(binding = L_Instance, std430) readonly buffer Mem  { uint    instanceMem[]; };
layout(binding = L_Payload,  std430) readonly buffer Pbo  { uvec4   payload[];     };
layout(binding = L_Bucket,   std140) readonly buffer Bbo  { Bucket  bucket[];      };
#endif

#if !defined(CLUSTER) && (MESH_TYPE!=T_PFX)
layout(binding = L_Ibo,      std430) readonly buffer Ibo  { uint    indexes [];    } ibo[];
layout(binding = L_Vbo,      std430) readonly buffer Vbo  { float   vertices[];    } vbo[];
#endif

#if defined(GL_FRAGMENT_SHADER) && defined(MAT_UV)
layout(binding = L_Diffuse)          uniform  texture2D textureMain[];
layout(binding = L_Sampler)          uniform  sampler   samplerMain;
#endif

#if (MESH_TYPE==T_MORPH)
layout(binding = L_MorphId,  std430) readonly buffer MId  { int     index[];       } morphId[];
layout(binding = L_Morph,    std430) readonly buffer MSmp { vec4    samples[];     } morph[];
#endif

#if defined(GL_FRAGMENT_SHADER) && defined(FORWARD) && !defined(DEPTH_ONLY)
layout(binding = L_Shadow0)          uniform sampler2D textureSm0;
layout(binding = L_Shadow1)          uniform sampler2D textureSm1;
#endif

#if defined(GL_FRAGMENT_SHADER) && (defined(WATER) || defined(GHOST))
layout(binding = L_SceneClr)         uniform sampler2D sceneColor;
layout(binding = L_GDepth  )         uniform sampler2D gbufferDepth;
#endif

#if !defined(CLUSTER) && (MESH_TYPE!=T_PFX)
mat4 pullMatrix(uint i) {
  i *= 16;
  mat4 ret;
  ret[0][0] = uintBitsToFloat(instanceMem[i+0]);
  ret[0][1] = uintBitsToFloat(instanceMem[i+1]);
  ret[0][2] = uintBitsToFloat(instanceMem[i+2]);
  ret[0][3] = uintBitsToFloat(instanceMem[i+3]);
  ret[1][0] = uintBitsToFloat(instanceMem[i+4]);
  ret[1][1] = uintBitsToFloat(instanceMem[i+5]);
  ret[1][2] = uintBitsToFloat(instanceMem[i+6]);
  ret[1][3] = uintBitsToFloat(instanceMem[i+7]);
  ret[2][0] = uintBitsToFloat(instanceMem[i+8]);
  ret[2][1] = uintBitsToFloat(instanceMem[i+9]);
  ret[2][2] = uintBitsToFloat(instanceMem[i+10]);
  ret[2][3] = uintBitsToFloat(instanceMem[i+11]);
  ret[3][0] = uintBitsToFloat(instanceMem[i+12]);
  ret[3][1] = uintBitsToFloat(instanceMem[i+13]);
  ret[3][2] = uintBitsToFloat(instanceMem[i+14]);
  ret[3][3] = uintBitsToFloat(instanceMem[i+15]);
  return ret;
  }

Instance pullInstance(uint i) {
  i *= 16;
  Instance ret;
  ret.mat[0][0] = uintBitsToFloat(instanceMem[i+0]);
  ret.mat[0][1] = uintBitsToFloat(instanceMem[i+1]);
  ret.mat[0][2] = uintBitsToFloat(instanceMem[i+2]);
  ret.mat[1][0] = uintBitsToFloat(instanceMem[i+3]);
  ret.mat[1][1] = uintBitsToFloat(instanceMem[i+4]);
  ret.mat[1][2] = uintBitsToFloat(instanceMem[i+5]);
  ret.mat[2][0] = uintBitsToFloat(instanceMem[i+6]);
  ret.mat[2][1] = uintBitsToFloat(instanceMem[i+7]);
  ret.mat[2][2] = uintBitsToFloat(instanceMem[i+8]);
  ret.mat[3][0] = uintBitsToFloat(instanceMem[i+9]);
  ret.mat[3][1] = uintBitsToFloat(instanceMem[i+10]);
  ret.mat[3][2] = uintBitsToFloat(instanceMem[i+11]);
  ret.fatness   = uintBitsToFloat(instanceMem[i+12]);
  ret.animPtr   = instanceMem[i+13];
  return ret;
  }

MorphDesc pullMorphDesc(uint i) {
  i *= 4;
  MorphDesc ret;
  ret.indexOffset         = instanceMem[i+0];
  ret.sample0             = instanceMem[i+1];
  ret.sample1             = instanceMem[i+2];
  ret.alpha16_intensity16 = instanceMem[i+3];
  return ret;
  }

vec3 pullPosition(uint instanceId) {
#if defined(LVL_OBJECT)
  return pullInstance(instanceId).mat[3];
#else
  return vec3(0);
#endif
  }
#endif

#endif
