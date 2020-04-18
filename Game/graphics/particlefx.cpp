#include "particlefx.h"

#include "resources.h"

using namespace Tempest;

ParticleFx::ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX &src, const char* name)
  :dbgName(name) {
  ppsValue            = src.ppsValue;
  ppsScaleKeys_S      = src.ppsScaleKeys_S.c_str();
  ppsIsLooping        = src.ppsIsLooping!=0;
  ppsIsSmooth         = src.ppsIsSmooth!=0;
  ppsFPS              = src.ppsFPS;
  ppsCreateEm_S       = src.ppsCreateEm_S.c_str();
  ppsCreateEmDelay    = src.ppsCreateEmDelay;

  shpType_S           = loadEmitType(src.shpType_S);
  shpFOR_S            = src.shpFOR_S.c_str();
  shpOffsetVec_S      = loadVec3(src.shpOffsetVec_S);
  shpDistribType_S    = src.shpDistribType_S.c_str();
  shpDistribWalkSpeed = src.shpDistribWalkSpeed;
  shpIsVolume         = src.shpIsVolume!=0;
  shpDim_S            = loadVec3(src.shpDim_S);
  shpMesh_S           = src.shpMesh_S.c_str();
  shpMeshRender_B     = src.shpMeshRender_B!=0;
  shpScaleKeys_S      = src.shpScaleKeys_S.c_str();
  shpScaleIsLooping   = src.shpScaleIsLooping!=0;
  shpScaleIsSmooth    = src.shpScaleIsSmooth!=0;
  shpScaleFPS         = src.shpScaleFPS;

  dirMode_S           = loadDirType(src.dirMode_S);
  dirFOR_S            = src.dirFOR_S.c_str();
  dirModeTargetFOR_S  = src.dirModeTargetFOR_S.c_str();
  dirModeTargetPos_S  = loadVec3(src.dirModeTargetPos_S);
  dirAngleHead        = src.dirAngleHead;
  dirAngleHeadVar     = src.dirAngleHeadVar;
  dirAngleElev        = src.dirAngleElev;
  dirAngleElevVar     = src.dirAngleElevVar;

  velAvg              = src.velAvg;
  velVar              = src.velVar;
  lspPartAvg          = src.lspPartAvg;
  lspPartVar          = src.lspPartVar;

  flyGravity_S        = loadVec3(src.flyGravity_S);
  flyCollDet_B        = src.flyCollDet_B!=0;

  visName_S           = loadTexture(src.visName_S.c_str());
  visOrientation_S    = src.visOrientation_S.c_str();
  visTexIsQuadPoly    = src.visTexIsQuadPoly;
  visTexAniFPS        = src.visTexAniFPS;
  visTexAniIsLooping  = src.visTexAniIsLooping;
  visTexColorStart_S  = loadVec3(src.visTexColorStart_S);
  visTexColorEnd_S    = loadVec3(src.visTexColorEnd_S);
  visSizeStart_S      = loadVec2(src.visSizeStart_S);
  visSizeEndScale     = src.visSizeEndScale;
  visAlphaFunc_S      = loadAlphaFn(src.visAlphaFunc_S);
  visAlphaStart       = src.visAlphaStart/255.f;
  visAlphaEnd         = src.visAlphaEnd/255.f;

   trlFadeSpeed       = src.trlFadeSpeed;
   trlTexture_S       = src.trlTexture_S.c_str();
   trlWidth           = src.trlWidth;

   mrkFadeSpeed       = src.mrkFadeSpeed;
   mrkTexture_S       = src.mrkTexture_S.c_str();
   mrkSize            = src.mrkSize;

  flockMode           = src.flockMode.c_str();
  flockStrength       = src.flockStrength;

  useEmittersFOR      = src.useEmittersFOR!=0;

  timeStartEnd_S      = src.timeStartEnd_S.c_str();
  m_bIsAmbientPFX     = src.m_bIsAmbientPFX!=0;
  }

uint64_t ParticleFx::maxLifetime() const {
  return uint64_t(lspPartAvg+lspPartVar);
  }

Vec2 ParticleFx::loadVec2(const Daedalus::ZString& src) {
  if(src=="=")
    return Vec2();

  float       v[2] = {};
  const char* str  = src.c_str();
  for(int i=0;i<2;++i) {
    char* next=nullptr;
    v[i] = std::strtof(str,&next);
    if(str==next) {
      if(i==1)
        return Vec2(v[0],v[0]);
      }
    str = next;
    }
  return Vec2(v[0],v[1]);
  }

const Tempest::Texture2d* ParticleFx::loadTexture(const std::string &src) {
  auto view = Resources::loadTexture(src);
  if(view==nullptr)
    view = &Resources::fallbackBlack();
  return view;
  }

Vec3 ParticleFx::loadVec3(const Daedalus::ZString& src) {
  if(src=="=")
    return Vec3();

  float       v[3] = {};
  const char* str  = src.c_str();
  for(int i=0;i<3;++i) {
    char* next=nullptr;
    v[i] = std::strtof(str,&next);
    if(str==next) {
      if(i==1)
        return Vec3(v[0],v[0],v[0]);
      if(i==2)
        return Vec3(v[0],v[1],0.f);
      }
    str = next;
    }
  return Vec3(v[0],v[1],v[2]);
  }

ParticleFx::EmitterType ParticleFx::loadEmitType(const Daedalus::ZString& src) {
  if(src=="POINT")
    return EmitterType::Point;
  if(src=="LINE")
    return EmitterType::Line;
  if(src=="BOX")
    return EmitterType::Box;
  if(src=="CYRCLE")
    return EmitterType::Circle;
  if(src=="SPHERE")
    return EmitterType::Sphere;
  if(src=="MESH")
    return EmitterType::Mesh;
  return EmitterType::Point;
  }

ParticleFx::Dir ParticleFx::loadDirType(const Daedalus::ZString& src) {
  if(src=="RAND")
    return Dir::Rand;
  if(src=="DIR")
    return Dir::Dir;
  if(src=="TARGET")
    return Dir::Target;
  return Dir::Rand;
  }

ParticleFx::AlphaFunc ParticleFx::loadAlphaFn(const Daedalus::ZString& src) {
  if(src=="NONE")
    return AlphaFunc::None;
  if(src=="BLEND")
    return AlphaFunc::Blend;
  if(src=="ADD")
    return AlphaFunc::Add;
  if(src=="MUL")
    return AlphaFunc::Mul;
  return AlphaFunc::None;
  }
