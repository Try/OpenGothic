#ifndef SCENE_GLSL
#define SCENE_GLSL

// std140, because uniform buffer
struct SceneDesc {
  vec3  sunDir;
  float waveAnim;
  mat4  viewProject;
  mat4  viewProjectInv;
  mat4  viewShadow[2];
  vec3  ambient;
  float exposureInv;
  vec3  sunCl;
  float GSunIntensity;
  vec4  frustrum[6];
  vec3  clipInfo;
  uint  tickCount32;
  vec3  camPos;
  float isNight;
  vec2  screenResInv;
  vec2  closeupShadowSlice;
  vec3  pfxLeft;
  uint  underWater;
  vec3  pfxTop;
  // float padd2;
  vec3  pfxDepth;
  // float padd3;
  ivec2 hiZTileSize;
  ivec2 screenRes;
  vec4  cloudsDir;
  };

#endif
