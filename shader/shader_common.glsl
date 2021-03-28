#define MAX_NUM_SKELETAL_NODES 96

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

#if defined(OBJ)
layout(push_constant, std140) uniform UboPush {
  mat4  obj;
  int   samplesPerFrame;
  int   morphFrame0;
  int   morphFrame1;
  float morphAlpha;
  } push;
#endif

#if defined(FRAGMENT) && !(defined(SHADOW_MAP) && !defined(ATEST))
layout(binding = 0) uniform sampler2D textureD;
#endif

#if defined(FRAGMENT) && !defined(SHADOW_MAP)
layout(binding = 1) uniform sampler2D textureSm;
#endif

layout(binding = 2, std140) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  mv;
  mat4  modelViewInv;
  mat4  shadow[2];
  vec3  ambient;
  vec4  sunCl;
  } scene;

#if defined(SKINING) && defined(VERTEX)
layout(binding = 3, std140) uniform UboAnim {
  mat4 skel[MAX_NUM_SKELETAL_NODES];
  } anim;
#endif

#if defined(VERTEX) && defined(OBJ)
layout(binding = 4, std140) uniform UboMaterial {
  vec2 texAnim;
  } material;
#endif

#if defined(FRAGMENT) && (defined(WATER) || defined(GHOST))
layout(binding = 5) uniform sampler2D gbufferDiffuse;
layout(binding = 6) uniform sampler2D gbufferDepth;
#endif

#if defined(VERTEX) && defined(MORPH)
layout(binding = 7, std140) readonly buffer SsboMorphId {
  ivec4 index[];
  } morphId;
layout(binding = 8, std140) readonly buffer SsboMorph {
  vec4  samples[];
  } morph;
#endif
