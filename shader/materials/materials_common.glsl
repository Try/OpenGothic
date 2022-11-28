#ifndef MATERIALS_COMMON_GLSL
#define MATERIALS_COMMON_GLSL

#include "../common.glsl"
#include "../scene.glsl"

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
#define MAX_MORPH_LAYERS       3
#define MaxVert                64
#define MaxInd                 (41*3)

#define T_LANDSCAPE 0
#define T_OBJ       1
#define T_SKINING   2
#define T_MORPH     3
#define T_PFX       4

#define L_Scene    0
#define L_Matrix   1
#define L_MeshDesc L_Matrix
#define L_Bucket   2
#define L_Ibo      3
#define L_Vbo      4
#define L_Diffuse  5
#define L_Shadow0  6
#define L_Shadow1  7
#define L_MorphId  8
#define L_Morph    9
#define L_GDiffuse 10
#define L_GDepth   11
#define L_HiZ      12
#define L_SkyLut   13

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING || MESH_TYPE==T_MORPH)
#define LVL_OBJECT 1
#endif

#if defined(HIZ) || defined(SHADOW_MAP)
#define DEPTH_ONLY 1
#endif

#if !defined(DEPTH_ONLY) || defined(ATEST)
#define MAT_UV 1
#endif

#if !defined(DEPTH_ONLY) && (MESH_TYPE==T_PFX)
#define MAT_COLOR 1
#endif

#if defined(MAT_UV) || !defined(DEPTH_ONLY) || defined(WATER) || defined(MAT_COLOR)
#define MAT_VARYINGS 1
#endif

struct Varyings {
#if defined(MAT_UV)
  vec2 uv;
#endif

#if !defined(DEPTH_ONLY)
  vec3 normal;
  vec3 pos;
#endif

#if defined(MAT_COLOR)
  vec4 color;
#endif

#if !defined(MAT_VARYINGS)
  float dummy;
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

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING)
layout(push_constant, std430) uniform UboPush {
  uint      meshletBase;
  uint      meshletCount;
  float     fatness;
  } push;
#elif (MESH_TYPE==T_MORPH)
layout(push_constant, std430) uniform UboPush {
  uint      meshletBase;
  uint      meshletCount;
  float     fatness;
  uint      padd0;

  MorphDesc morph[MAX_MORPH_LAYERS];
  } push;
#elif (MESH_TYPE==T_PFX || MESH_TYPE==T_LANDSCAPE)
// no push
#else
#error "unknown MESH_TYPE"
#endif

layout(binding = L_Scene, std140) uniform UboScene {
  SceneDesc scene;
  };

#if defined(LVL_OBJECT) && (defined(VERTEX) || defined(MESH) || defined(TASK))
layout(binding = L_Matrix, std430)   readonly buffer Matrix { mat4 matrix[]; };
#endif

#if (MESH_TYPE==T_LANDSCAPE) && (defined(VERTEX) || defined(MESH) || defined(TASK))
layout(binding = L_MeshDesc, std430) readonly buffer Inst   { vec4 bounds[]; };
#endif

#if (defined(LVL_OBJECT) || defined(WATER))
layout(binding = L_Bucket, std140) uniform BucketDesc {
  vec4  bbox[2];
  ivec2 texAniMapDirPeriod;
  float bboxRadius;
  float waveMaxAmplitude;
  } bucket;
#endif

#if defined(MESH) || defined(TASK)
layout(binding = L_Ibo, std430)      readonly buffer Ibo  { uint  indexes []; };
layout(binding = L_Vbo, std430)      readonly buffer Vbo  { float vertices[]; };
#endif

#if defined(FRAGMENT) && !(defined(DEPTH_ONLY) && !defined(ATEST))
layout(binding = L_Diffuse) uniform sampler2D textureD;
#endif

#if defined(FRAGMENT) && defined(FORWARD) && !defined(DEPTH_ONLY)
layout(binding = L_Shadow0) uniform sampler2D textureSm0;
layout(binding = L_Shadow1) uniform sampler2D textureSm1;
#endif

#if (MESH_TYPE==T_MORPH) && (defined(VERTEX) || defined(MESH))
layout(std430, binding = L_MorphId) readonly buffer SsboMorphId {
  int  index[];
  } morphId;
layout(std430, binding = L_Morph) readonly buffer SsboMorph {
  vec4 samples[];
  } morph;
#endif

#if defined(FRAGMENT) && (defined(WATER) || defined(GHOST))
layout(binding = L_GDiffuse) uniform sampler2D gbufferDiffuse;
layout(binding = L_GDepth  ) uniform sampler2D gbufferDepth;
#endif

#if defined(MESH) && !defined(SHADOW_MAP)
layout(binding = L_HiZ)  uniform sampler2D hiZ;
#endif

#if defined(FRAGMENT) && defined(WATER)
layout(binding = L_SkyLut) uniform sampler2D skyLUT;
#endif

#endif
