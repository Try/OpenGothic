#include "particlefx.h"

#include "resources.h"

using namespace Tempest;

ParticleFx::ParticleFx(const Texture2d* spr, const ZenLoad::zCVobData& vob) {
  ppsValue         = -1;
  lspPartAvg       = 1000;
  dirMode          = ParticleFx::Dir::Dir;
  visName_S        = spr;
  visAlphaFunc     = ParticleFx::AlphaFunc::Add;
  visTexColorStart = Vec3(255,255,255);
  visTexColorEnd   = Vec3(255,255,255);
  visSizeStart     = Vec2(2.f*vob.visualChunk.zCDecal.decalDim.x,
                          2.f*vob.visualChunk.zCDecal.decalDim.y);
  visOrientation   = Orientation::Velocity;
  dirFOR           = Frame::World;
  dirAngleElev     = 90;
  visSizeEndScale  = 1;
  visAlphaStart    = 1;
  visAlphaEnd      = 1;
  visYawAlign      = vob.visualCamAlign==1;
  visZBias         = vob.zBias!=0;
  }

ParticleFx::ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX &src, const char* name)
  :dbgName(name) {
  ppsValue            = std::max(0.f,src.ppsValue);
  ppsScaleKeys        = loadArr(src.ppsScaleKeys_S);
  ppsIsLooping        = src.ppsIsLooping!=0;
  ppsIsSmooth         = src.ppsIsSmooth!=0;
  ppsFPS              = src.ppsFPS;
  ppsCreateEm_S       = src.ppsCreateEm_S.c_str();
  ppsCreateEmDelay    = src.ppsCreateEmDelay;

  shpType             = loadEmitType(src.shpType_S);
  shpFOR              = loadFrameType(src.shpFOR_S);
  shpOffsetVec        = loadVec3(src.shpOffsetVec_S);
  shpDistribType      = loadDistribType(src.shpDistribType_S);
  shpDistribWalkSpeed = src.shpDistribWalkSpeed;
  shpIsVolume         = src.shpIsVolume!=0;
  shpDim              = loadVec3(src.shpDim_S);
  shpMesh             = Resources::loadEmiterMesh(src.shpMesh_S.c_str());
  shpMeshRender       = src.shpMeshRender_B!=0;
  shpScaleKeys        = loadArr(src.shpScaleKeys_S);
  shpScaleIsLooping   = src.shpScaleIsLooping!=0;
  shpScaleIsSmooth    = src.shpScaleIsSmooth!=0;
  shpScaleFPS         = src.shpScaleFPS;

  dirMode             = loadDirType(src.dirMode_S);
  dirFOR              = loadFrameType(src.dirFOR_S);
  dirModeTargetFOR    = loadFrameType(src.dirModeTargetFOR_S);
  dirModeTargetPos    = loadVec3(src.dirModeTargetPos_S);
  dirAngleHead        = src.dirAngleHead;
  dirAngleHeadVar     = src.dirAngleHeadVar;
  dirAngleElev        = src.dirAngleElev;
  dirAngleElevVar     = src.dirAngleElevVar;

  velAvg              = src.velAvg;
  velVar              = src.velVar;
  lspPartAvg          = src.lspPartAvg;
  lspPartVar          = src.lspPartVar;

  flyGravity          = loadVec3(src.flyGravity_S);
  flyCollDet          = src.flyCollDet_B!=0;

  visName_S           = loadTexture(src.visName_S.c_str());
  visOrientation      = loadOrientation(src.visOrientation_S);
  visTexIsQuadPoly    = src.visTexIsQuadPoly;
  visTexAniFPS        = src.visTexAniFPS;
  visTexAniIsLooping  = src.visTexAniIsLooping;
  visTexColorStart    = loadVec3(src.visTexColorStart_S);
  visTexColorEnd      = loadVec3(src.visTexColorEnd_S);
  visSizeStart        = loadVec2(src.visSizeStart_S);
  visSizeEndScale     = src.visSizeEndScale;
  visAlphaFunc        = loadAlphaFn(src.visAlphaFunc_S);
  visAlphaStart       = src.visAlphaStart/255.f;
  visAlphaEnd         = src.visAlphaEnd/255.f;

  trlFadeSpeed       = src.trlFadeSpeed;
  trlTexture         = loadTexture(src.trlTexture_S.c_str());
  trlWidth           = src.trlWidth;

  mrkFadeSpeed       = src.mrkFadeSpeed;
  mrkTexture         = loadTexture(src.mrkTexture_S.c_str());
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

uint64_t ParticleFx::effectPrefferedTime() const {
  if(ppsScaleKeys.size()==0)
    return 5000;

  auto sec = ppsScaleKeys.size()/std::max<size_t>(1,size_t(std::ceil(ppsFPS)));
  return sec*1000;
  }

float ParticleFx::maxPps() const {
  if(ppsValue<0)
    return 1; // permanent particles

  if(ppsScaleKeys.size()==0)
    return ppsValue;

  float v = 0;
  for(auto i:ppsScaleKeys)
    v = std::max(i,v);
  return v*ppsValue;
  }

float ParticleFx::shpScale(uint64_t time) const {
  return fetchScaleKey(time,shpScaleKeys,shpScaleFPS,
                       shpScaleIsSmooth!=0,shpScaleIsLooping!=0);
  }

float ParticleFx::ppsScale(uint64_t time) const {
  float v = fetchScaleKey(time,ppsScaleKeys,ppsFPS,ppsIsSmooth!=0,ppsIsLooping!=0);
  if(v<0)
    return 0.f;
  return v;
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

const Tempest::Texture2d* ParticleFx::loadTexture(const char* src) {
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

ParticleFx::KeyList ParticleFx::loadArr(const Daedalus::ZString& src) {
  std::vector<float> v;
  const char* str  = src.c_str();
  for(int i=0;;++i) {
    char* next=nullptr;
    float f = std::strtof(str,&next);
    if(str==next) {
      return v;
      }
    v.push_back(f);
    str = next;
    }
  }

ParticleFx::EmitterType ParticleFx::loadEmitType(const Daedalus::ZString& src) {
  if(src=="POINT")
    return EmitterType::Point;
  if(src=="LINE")
    return EmitterType::Line;
  if(src=="BOX")
    return EmitterType::Box;
  if(src=="CIRCLE")
    return EmitterType::Circle;
  if(src=="SPHERE")
    return EmitterType::Sphere;
  if(src=="MESH")
    return EmitterType::Mesh;
  return EmitterType::Point;
  }

ParticleFx::Frame ParticleFx::loadFrameType(const Daedalus::ZString& src) {
  if(src=="OBJECT" || src=="object")
    return Frame::Object;
  if(src=="WORLD")
    return Frame::World;
  return Frame::Object;
  }

ParticleFx::Distribution ParticleFx::loadDistribType(const Daedalus::ZString& src) {
  if(src=="RAND" || src=="RANDOM")
    return Distribution::Rand;
  if(src=="DIR")
    return Distribution::Dir;
  if(src=="UNIFORM")
    return Distribution::Uniform;
  if(src=="WALK" || src=="WALKW")
    return Distribution::Walk;
  return Distribution::Rand;
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

ParticleFx::Orientation ParticleFx::loadOrientation(const Daedalus::ZString& src) {
  if(src=="NONE")
    return Orientation::None;
  if(src=="VELO")
    return Orientation::Velocity;
  if(src=="VELO3D")
    return Orientation::Velocity3d;
  return Orientation::None;
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

float ParticleFx::fetchScaleKey(uint64_t time, const KeyList& keys, float fps, bool smooth, bool loop) const {
  if(keys.size()==0)
    return 1.f;

  uint64_t timeScaled = uint64_t(fps*float(time));
  size_t   at         = size_t(timeScaled/1000);
  float    alpha      = float(timeScaled%1000)/1000.f;
  if(smooth)
    alpha = alpha<0.5f ? 0.f : 1.f;

  size_t frameA = 0, frameB = 0;
  if(loop) {
    frameA = (at  )%keys.size();
    frameB = (at+1)%keys.size();
    } else {
    frameA = std::min(at,  keys.size()-1);
    frameB = std::min(at+1,keys.size()-1);
    }
  float k0 = keys[frameA];
  float k1 = keys[frameB];
  return k0+alpha*(k1-k0);
  }
