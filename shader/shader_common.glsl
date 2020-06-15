#define LIGHT_CNT              6
#define MAX_NUM_SKELETAL_NODES 96

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

#if defined(FRAGMENT)
layout(binding = 0) uniform sampler2D textureD;
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

#if defined(OBJ)
layout(std140,binding = 3) uniform UboObject {
  mat4  obj;
  Light light[LIGHT_CNT];
  } ubo;
#endif

#if defined(SKINING) && defined(VERTEX)
layout(std140,binding = 4) uniform UboAnim {
  mat4 skel[MAX_NUM_SKELETAL_NODES];
  } anim;
#endif
