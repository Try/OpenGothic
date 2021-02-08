#define LIGHT_BLOCK            2
#define MAX_NUM_SKELETAL_NODES 96

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

#if defined(OBJ)
layout(std140,push_constant) uniform UboPush {
  mat4  obj;
  Light light[LIGHT_BLOCK];
  } push;
#endif

#if defined(FRAGMENT) && !(defined(SHADOW_MAP) && !defined(ATEST))
layout(binding = 0) uniform sampler2D textureD;
#endif
#if defined(FRAGMENT) && !defined(SHADOW_MAP)
layout(binding = 1) uniform sampler2D textureSm;
#endif
#if defined(FRAGMENT) && (defined(WATER) || defined(GHOST))
layout(binding = 5) uniform sampler2D gbufferDiffuse;
layout(binding = 6) uniform sampler2D gbufferDepth;
#endif

layout(std140,binding = 2) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  mv;
  mat4  modelViewInv;
  mat4  shadow;
  vec3  ambient;
  vec4  sunCl;
  } scene;

#if defined(SKINING) && defined(VERTEX)
layout(std140,binding = 3) uniform UboAnim {
  mat4 skel[MAX_NUM_SKELETAL_NODES];
  } anim;
#endif

#if defined(VERTEX) && defined(OBJ)
layout(std140,binding = 4) uniform UboMaterial {
  vec2 texAnim;
  } material;
#endif
