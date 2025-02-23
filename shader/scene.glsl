#ifndef SCENE_GLSL
#define SCENE_GLSL

// std140, because uniform buffer
struct SceneDesc {
  mat4  viewProject;
  mat4  viewProjectInv;
  mat4  viewShadow[2];
  mat4  viewProjectLwcInv;
  mat4  viewShadowLwc[2];
  mat4  viewVirtualShadow;
  mat4  viewVirtualShadowLwc;
  vec4  vsmDdx, vsmDdy;
  mat4  view;
  mat4  project;
  mat4  projectInv;
  vec3  sunDir;
  float waveAnim;
  vec3  ambient;
  float exposure;
  vec3  sunColor;
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
  float luminanceMed; // for debugging
  vec3  pfxDepth;
  float plPosY;
  ivec2 hiZTileSize;
  ivec2 screenRes;
  vec4  cloudsDir;
  };

struct LightSource {
  vec3  pos;
  float range;
  vec3  color;
  float padd0;
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
  uint  vertexCount;
  uint  instanceCount;
  uint  firstVertex;
  uint  firstInstance;
  uint  writeOffset;
  };

struct Cluster {
  vec4  sphere;
  uint  bucketId_commandId;
  uint  firstMeshlet;
  int   meshletCount;
  uint  instanceId;
  };

const uint BK_SOLID = 0x1;
const uint BK_SKIN  = 0x2;
const uint BK_MORPH = 0x4;

struct Bucket {
  vec4  bbox[2];
  ivec2 texAniMapDirPeriod;
  float bboxRadius;
  float waveMaxAmplitude;
  float alphaWeight;
  float envMapping;
  uint  flags;
  };

#endif
