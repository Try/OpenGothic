#define LIGHT_INLINE           2
#define LIGHT_BLOCK            4
#define MAX_NUM_SKELETAL_NODES 96

#if defined(LIGHT_EXT)
#define LIGHT_CNT LIGHT_BLOCK
#else
#define LIGHT_CNT LIGHT_INLINE
#endif

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

#if defined(OBJ) && defined(FRAGMENT)
layout(std140,push_constant) uniform UboObject {
#if defined(LIGHT_EXT)
  Light light[LIGHT_BLOCK];
#else
  Light light[LIGHT_INLINE];
#endif
  } push;
#endif

#if defined(FRAGMENT)
layout(binding = 0) uniform sampler2D textureD;
#endif
#if defined(FRAGMENT)
layout(binding = 1) uniform sampler2D textureSm;
#endif

layout(std140,binding = 2) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  mv;
  mat4  shadow;
  vec3  ambient;
  vec4  sunCl;
  } scene;

#if defined(OBJ) && defined(VERTEX)
layout(std140,binding = 3) uniform UboObject {
  mat4  obj;
  } ubo;
#endif

#if defined(SKINING) && defined(VERTEX)
layout(std140,binding = 4) uniform UboAnim {
  mat4 skel[MAX_NUM_SKELETAL_NODES];
  } anim;
#endif
