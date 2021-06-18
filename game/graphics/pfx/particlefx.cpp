#include "particlefx.h"

#include "utils/parser.h"
#include "resources.h"

using namespace Tempest;

ParticleFx::ParticleFx(const Material& mat, const ZenLoad::zCVobData& vob) {
  dbgName          = vob.visual;

  ppsValue         = -1;
  lspPartAvg       = 1000;
  dirMode          = ParticleFx::Dir::Dir;
  visTexColorStart = Vec3(255,255,255);
  visTexColorEnd   = Vec3(255,255,255);
  visSizeStart     = Vec2(2.f*vob.visualChunk.zCDecal.decalDim.x,
                          2.f*vob.visualChunk.zCDecal.decalDim.y);
  visOrientation   = Orientation::None;

  visMaterial      = mat;

  shpFOR           = Frame::World;

  dirFOR           = Frame::World;
  dirAngleElev     = 90;
  visSizeEndScale  = 1;
  visAlphaStart    = 1;
  visAlphaEnd      = 1;
  visYawAlign      = vob.visualCamAlign==1;
  visZBias         = vob.zBias!=0;

  useEmittersFOR   = true;
  }

ParticleFx::ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX &src, const char* name)
  :dbgName(name) {
  ppsValue            = std::max(0.f,src.ppsValue);
  ppsScaleKeys        = loadArr(src.ppsScaleKeys_S);
  ppsIsLooping        = src.ppsIsLooping!=0;
  ppsIsSmooth         = src.ppsIsSmooth!=0;
  ppsFPS              = src.ppsFPS;
  // ppsCreateEm = ; // assign externaly
  ppsCreateEmDelay    = uint64_t(src.ppsCreateEmDelay);

  shpType             = loadEmitType(src.shpType_S);
  shpFOR              = loadFrameType(src.shpFOR_S);
  shpOffsetVec        = Parser::loadVec3(src.shpOffsetVec_S);
  shpDistribType      = loadDistribType(src.shpDistribType_S);
  shpDistribWalkSpeed = src.shpDistribWalkSpeed;
  shpIsVolume         = src.shpIsVolume!=0;
  shpDim              = Parser::loadVec3(src.shpDim_S);
  shpMesh_S           = src.shpMesh_S;
  shpMesh             = Resources::loadEmiterMesh(src.shpMesh_S.c_str());
  shpMeshRender       = src.shpMeshRender_B!=0;
  shpScaleKeys        = loadArr(src.shpScaleKeys_S);
  shpScaleIsLooping   = src.shpScaleIsLooping!=0;
  shpScaleIsSmooth    = src.shpScaleIsSmooth!=0;
  shpScaleFPS         = src.shpScaleFPS;

  dirMode             = loadDirType(src.dirMode_S);
  dirFOR              = loadFrameType(src.dirFOR_S);
  dirModeTargetFOR    = loadFrameType(src.dirModeTargetFOR_S);
  dirModeTargetPos    = Parser::loadVec3(src.dirModeTargetPos_S);
  dirAngleHead        = src.dirAngleHead;
  dirAngleHeadVar     = src.dirAngleHeadVar;
  dirAngleElev        = src.dirAngleElev;
  dirAngleElevVar     = src.dirAngleElevVar;

  velAvg              = src.velAvg;
  velVar              = src.velVar;
  lspPartAvg          = src.lspPartAvg;
  lspPartVar          = src.lspPartVar;

  flyGravity          = Parser::loadVec3(src.flyGravity_S);
  flyCollDet          = src.flyCollDet_B!=0;

  visMaterial         = Material(src);
  visOrientation      = loadOrientation(src.visOrientation_S);
  visTexIsQuadPoly    = src.visTexIsQuadPoly;
  visTexAniFPS        = src.visTexAniFPS;
  visTexAniIsLooping  = src.visTexAniIsLooping;
  visTexColorStart    = Parser::loadVec3(src.visTexColorStart_S);
  visTexColorEnd      = Parser::loadVec3(src.visTexColorEnd_S);
  visSizeStart        = Parser::loadVec2(src.visSizeStart_S);
  visSizeEndScale     = src.visSizeEndScale;
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

  prefferedTime       = calcPrefferedTimeSingle();
  }

ParticleFx::ParticleFx(const Daedalus::GEngineClasses::C_ParticleFXEmitKey& src, const Daedalus::GEngineClasses::C_ParticleFX& proto) {
  *this = ParticleFx(proto,src.visName_S.c_str());

  if(!src.pfx_shpDim_S.empty())
    shpDim            = Parser::loadVec3(src.pfx_shpDim_S);

  shpIsVolume         = src.pfx_shpIsVolumeChg!=0;

  if(src.pfx_shpScaleFPS>0)
    shpScaleFPS       = src.pfx_shpScaleFPS;

  shpDistribWalkSpeed = src.pfx_shpDistribWalkSpeed;

  if(!src.pfx_shpOffsetVec_S.empty())
    shpOffsetVec      = Parser::loadVec3(src.pfx_shpOffsetVec_S);

  if(!src.pfx_shpDistribType_S.empty())
    shpDistribType    = loadDistribType(src.pfx_shpDistribType_S);

  if(!src.pfx_dirMode_S.empty())
    dirMode           = loadDirType(src.pfx_dirMode_S);

  if(!src.pfx_dirFOR_S.empty())
    dirFOR            = loadFrameType(src.pfx_dirFOR_S);

  if(!src.pfx_dirModeTargetFOR_S.empty())
    dirModeTargetFOR  = loadFrameType(src.pfx_dirModeTargetFOR_S);

  if(!src.pfx_dirModeTargetPos_S.empty())
    dirModeTargetPos  = Parser::loadVec3(src.pfx_dirModeTargetPos_S);

  if(src.pfx_velAvg>0)
    velAvg            = src.pfx_velAvg;

  if(src.pfx_lspPartAvg>0)
    lspPartAvg        = src.pfx_lspPartAvg;

  if(src.pfx_visAlphaStart>0)
    visAlphaStart     = src.pfx_visAlphaStart;
  }

uint64_t ParticleFx::maxLifetime() const {
  return uint64_t(lspPartAvg+lspPartVar);
  }

uint64_t ParticleFx::effectPrefferedTime() const {
  auto v0 = prefferedTime;
  auto v1 = ppsCreateEm==nullptr ? 0 : ppsCreateEmDelay+ppsCreateEm->effectPrefferedTime();
  return std::max(v0,v1);
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

uint64_t ParticleFx::calcPrefferedTimeSingle() const {
  if(ppsScaleKeys.size()==0)
    return 1;

  auto div = std::max<uint64_t>(1,uint64_t(std::ceil(ppsFPS)));
  auto sec = uint64_t(ppsScaleKeys.size()*1000)/div;
  return sec;
  }

const Tempest::Texture2d* ParticleFx::loadTexture(const char* src) {
  auto view = Resources::loadTexture(src);
  if(view==nullptr)
    view = &Resources::fallbackBlack();
  return view;
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
  if(src=="OBJECT")
    return Frame::Object;
  if(src=="object")
    return Frame::Node; // look like "object"(low case) is not valid token - should be interpreted differently
  if(src=="WORLD")
    return Frame::World;
  return Frame::World;
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

float ParticleFx::fetchScaleKey(uint64_t time, const KeyList& keys, float fps, bool smooth, bool loop) const {
  if(keys.size()==0)
    return 1.f;

  uint64_t timeScaled = uint64_t(fps*float(time));
  size_t   at         = size_t(timeScaled/1000);
  float    alpha      = float(timeScaled%1000)/1000.f;

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
  if(smooth)
    return k0+alpha*(k1-k0);
  if(alpha<0.5)
    return k0; else
    return k1;
  }
