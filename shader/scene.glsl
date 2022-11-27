#ifndef SCENE_GLSL
#define SCENE_GLSL

struct SceneDesc {
  vec3  sunDir;
  float waveAnim;
  mat4  viewProject;
  mat4  viewProjectInv;
  mat4  viewShadow[2];
  vec3  ambient;
  vec4  sunCl;
  vec4  frustrum[6];
  vec3  clipInfo;
  uint  tickCount32;
  vec3  camPos;
  // float padd2;
  vec2  screenResInv;
  vec2  closeupShadowSlice;
  };

#endif
