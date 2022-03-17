#include "../common.glsl"

#define MAX_NUM_SKELETAL_NODES 96
#define MAX_MORPH_LAYERS       3

#define T_LANDSCAPE 0
#define T_OBJ       1
#define T_SKINING   2
#define T_MORPH     3
#define T_PFX       4

#define L_Diffuse  0
#define L_Shadow0  1
#define L_Shadow1  2
#define L_Scene    3
#define L_Skinning 4
#define L_Material 5
#define L_GDiffuse 6
#define L_GDepth   7
#define L_MorphId  8
#define L_Morph    9

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING || MESH_TYPE==T_MORPH)
#define LVL_OBJECT 1
#endif

#if (defined(VERTEX) || defined(TESSELATION)) && (defined(LVL_OBJECT) || defined(WATER))
#define MAT_ANIM 1
#endif

struct Varyings {
  vec4 scr;
  vec2 uv;

#if !defined(SHADOW_MAP)
  vec4 shadowPos[2];
  vec3 normal;
#endif

#if !defined(SHADOW_MAP) || defined(WATER)
  vec3 pos;
#endif

#if !defined(SHADOW_MAP) && (MESH_TYPE==T_PFX)
  vec4 color;
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
  float alpha;
  };

#if defined(LVL_OBJECT)
layout(push_constant, std140) uniform UboPush {
  mat4      obj;
  float     fatness;
#if MESH_TYPE==T_MORPH
  MorphDesc morph[MAX_MORPH_LAYERS];
#endif
  } push;
#endif

#if defined(FRAGMENT) && !(defined(SHADOW_MAP) && !defined(ATEST))
layout(binding = L_Diffuse) uniform sampler2D textureD;
#endif

#if defined(FRAGMENT) && !defined(SHADOW_MAP)
layout(binding = L_Shadow0) uniform sampler2D textureSm0;
layout(binding = L_Shadow1) uniform sampler2D textureSm1;
#endif

layout(binding = L_Scene, std140) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  viewProject;
  mat4  viewProjectInv;
  mat4  shadow[2];
  vec3  ambient;
  vec4  sunCl;
  vec3  clipInfo;
  vec3  camPos;
  } scene;

#if (MESH_TYPE==T_SKINING) && defined(VERTEX)
layout(binding = L_Skinning, std140) uniform UboAnim {
  mat4 skel[MAX_NUM_SKELETAL_NODES];
  } anim;
#endif

#if defined(MAT_ANIM)
layout(binding = L_Material, std140) uniform UboMaterial {
  vec2  texAnim;
  float waveAnim;
  float waveMaxAmplitude;
  } material;
#endif

#if defined(FRAGMENT) && (defined(WATER) || defined(GHOST))
layout(binding = L_GDiffuse) uniform sampler2D gbufferDiffuse;
layout(binding = L_GDepth  ) uniform sampler2D gbufferDepth;
#endif

#if (MESH_TYPE==T_MORPH) && defined(VERTEX)
layout(binding = L_MorphId, std430) readonly buffer SsboMorphId {
  int  index[];
  } morphId;
layout(binding = L_Morph, std430) readonly buffer SsboMorph {
  vec4 samples[];
  } morph;
#endif
