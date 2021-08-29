#include "visualfx.h"

#include <Tempest/Log>
#include <cctype>

#include "world/world.h"
#include "utils/parser.h"
#include "gothic.h"

using namespace Tempest;

VisualFx::Key::Key(Daedalus::GEngineClasses::C_ParticleFXEmitKey&& k) {
  visName                 = Gothic::inst().loadParticleFx(k.visName_S.c_str());
  visSizeScale            = k.visSizeScale;
  scaleDuration           = k.scaleDuration; // time to reach full scale at this key for relevant vars (size, alpha, etc.)

  pfx_ppsValue            = k.pfx_ppsValue;
  pfx_ppsIsSmoothChg      = k.pfx_ppsIsSmoothChg!=0;
  pfx_ppsIsLoopingChg     = k.pfx_ppsIsLoopingChg!=0;
  pfx_scTime              = 0.f;
  if(!k.pfx_flyGravity_S.empty())
    pfx_flyGravity = Parser::loadVec3(k.pfx_flyGravity_S);

  if(!k.pfx_shpDim_S.empty())
    pfx_shpDim = Parser::loadVec3(k.pfx_shpDim_S);
  pfx_shpIsVolumeChg      = k.pfx_shpIsVolumeChg;    // changes volume rendering of pfx if set to 1
  pfx_shpScaleFPS         = k.pfx_shpScaleFPS;
  pfx_shpDistribWalkSpeed = k.pfx_shpDistribWalkSpeed;
  if(!k.pfx_shpOffsetVec_S.empty())
    pfx_shpOffsetVec = Parser::loadVec3(k.pfx_shpOffsetVec_S);
  pfx_shpDistribType_S    = k.pfx_shpDistribType_S;
  pfx_dirMode_S           = k.pfx_dirMode_S;
  pfx_dirFOR_S            = k.pfx_dirFOR_S;
  pfx_dirModeTargetFOR_S  = k.pfx_dirModeTargetFOR_S;
  pfx_dirModeTargetPos_S  = k.pfx_dirModeTargetPos_S;
  pfx_velAvg              = k.pfx_velAvg;
  pfx_lspPartAvg          = k.pfx_lspPartAvg;
  pfx_visAlphaStart       = k.pfx_visAlphaStart;

  lightPresetName         = k.lightPresetName;
  lightRange              = k.lightRange;
  sfxID                   = k.sfxID;
  sfxIsAmbient            = k.sfxIsAmbient;
  emCreateFXID            = Gothic::inst().loadVisualFx(k.emCreateFXID.c_str());

  emFlyGravity            = k.emFlyGravity;
  if(!k.emSelfRotVel_S.empty())
    emSelfRotVel          = Parser::loadVec3(k.emSelfRotVel_S);
  if(!k.emTrjMode_S.empty())
    emTrjMode             = VisualFx::loadTrajectory(k.emTrjMode_S);
  emTrjEaseVel            = k.emTrjEaseVel;
  emCheckCollision        = k.emCheckCollision!=0;
  emFXLifeSpan            = k.emFXLifeSpan<0 ? 0 : uint64_t(k.emFXLifeSpan*1000.f);
  }

VisualFx::VisualFx(const Daedalus::GEngineClasses::CFx_Base& fx, Daedalus::DaedalusVM& vm, std::string_view name) {
  visName_S                = fx.visName_S;
  visSize                  = Parser::loadVec2(fx.visSize_S);
  visAlpha                 = fx.visAlpha;
  visAlphaBlendFunc        = Parser::loadAlpha(fx.visAlphaBlendFunc_S);
  visTexAniFPS             = 0.f;
  visTexAniIsLooping       = false;

  emTrjMode                = loadTrajectory(fx.emTrjMode_S);
  emTrjOriginNode          = fx.emTrjOriginNode.c_str();
  emTrjTargetNode          = fx.emTrjTargetNode.c_str();
  emTrjTargetRange         = fx.emTrjTargetRange;
  emTrjTargetAzi           = fx.emTrjTargetAzi;
  emTrjTargetElev          = fx.emTrjTargetElev;
  emTrjNumKeys             = fx.emTrjNumKeys;
  emTrjNumKeysVar          = fx.emTrjNumKeysVar;
  emTrjAngleElevVar        = fx.emTrjAngleElevVar;
  emTrjAngleHeadVar        = fx.emTrjAngleHeadVar;
  emTrjKeyDistVar          = fx.emTrjKeyDistVar;
  emTrjLoopMode            = loadLoopmode(fx.emTrjLoopMode_S);
  emTrjEaseFunc            = loadEaseFunc(fx.emTrjEaseFunc_S);
  emTrjEaseVel             = fx.emTrjEaseVel;
  emTrjDynUpdateDelay      = fx.emTrjDynUpdateDelay;
  emTrjDynUpdateTargetOnly = fx.emTrjDynUpdateTargetOnly!=0;

  emFXCreate               = Gothic::inst().loadVisualFx(fx.emFXCreate_S.c_str());
  emFXInvestOrigin         = fx.emFXInvestOrigin_S.c_str();
  emFXInvestTarget         = fx.emFXInvestTarget_S.c_str();
  emFXTriggerDelay         = fx.emFXTriggerDelay<0 ? 0 : uint64_t(fx.emFXTriggerDelay*1000.f);
  emFXCreatedOwnTrj        = fx.emFXCreatedOwnTrj!=0;
  emActionCollDyn          = strToColision(fx.emActionCollDyn_S.c_str());		// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE
  emActionCollStat         = strToColision(fx.emActionCollStat_S.c_str());			// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE, CREATEQUAD
  emFXCollStat             = Gothic::inst().loadVisualFx(fx.emFXCollStat_S.c_str());
  emFXCollDyn              = Gothic::inst().loadVisualFx(fx.emFXCollDyn_S.c_str());
  emFXCollDynPerc          = Gothic::inst().loadVisualFx(fx.emFXCollDynPerc_S.c_str());
  emFXCollStatAlign        = loadCollisionAlign(fx.emFXCollStatAlign_S);
  emFXCollDynAlign         = loadCollisionAlign(fx.emFXCollDynAlign_S);
  emFXLifeSpan             = fx.emFXLifeSpan<0 ? 0 : uint64_t(fx.emFXLifeSpan*1000.f);

  emCheckCollision         = fx.emCheckCollision;
  emAdjustShpToOrigin      = fx.emAdjustShpToOrigin;
  emInvestNextKeyDuration  = fx.emInvestNextKeyDuration<0 ? 0 : uint64_t(fx.emInvestNextKeyDuration*1000.f);
  emFlyGravity             = fx.emFlyGravity;
  emSelfRotVel             = Parser::loadVec3(fx.emSelfRotVel_S);
  for(size_t i=0; i<Daedalus::GEngineClasses::VFX_NUM_USERSTRINGS; ++i)
    userString[i] = fx.userString[i];
  lightPresetName          = fx.lightPresetName;
  sfxID                    = fx.sfxID;
  sfxIsAmbient             = fx.sfxIsAmbient;
  sendAssessMagic          = fx.sendAssessMagic;
  secsPerDamage            = fx.secsPerDamage;

  for(auto& c:emTrjOriginNode)
    c = char(std::toupper(c));

  static const char* keyName[int(SpellFxKey::Count)] = {
    "OPEN",
    "INIT",
    "CAST",
    "INVEST",
    "COLLIDE"
    };

  for(int i=0; i<int(SpellFxKey::Count); ++i) {
    char kname[256]={};
    std::snprintf(kname,sizeof(kname),"%.*s_KEY_%s", int(name.size()),name.data(), keyName[i]);
    auto id = vm.getDATFile().getSymbolIndexByName(kname);
    if(id==size_t(-1))
      continue;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey key;
    vm.initializeInstance(key, id, Daedalus::IC_FXEmitKey);
    vm.clearReferences(Daedalus::IC_FXEmitKey);
    keys  [i] = Key(std::move(key));
    hasKey[i] = true;
    }

  for(int i=1; ; ++i) {
    char kname[256]={};
    std::snprintf(kname,sizeof(kname),"%.*s_KEY_INVEST_%d", int(name.size()),name.data(), i);
    auto id = vm.getDATFile().getSymbolIndexByName(kname);
    if(id==size_t(-1))
      break;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey key;
    vm.initializeInstance(key, id, Daedalus::IC_FXEmitKey);
    vm.clearReferences(Daedalus::IC_FXEmitKey);
    // keys[int(SpellFxKey::Invest)] = key;
    investKeys.push_back(std::move(key));
    }
  }

uint64_t VisualFx::effectPrefferedTime() const {
  return emFXLifeSpan;
  }

PfxEmitter VisualFx::visual(World& owner) const {
  return PfxEmitter(owner,visName_S.c_str());
  }

const VisualFx::Key* VisualFx::key(SpellFxKey type, int32_t keyLvl) const {
  if(type==SpellFxKey::Count)
    return nullptr;

  if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return &investKeys[size_t(keyLvl)];
    return nullptr;
    }

  if(!hasKey[int(type)])
    return nullptr;
  return &keys[int(type)];
  }

VisualFx::Trajectory VisualFx::loadTrajectory(const Daedalus::ZString& str) {
  uint8_t bits = 0;
  size_t  prev = 0;
  auto    s    = str.c_str();
  for(size_t i=0; i<str.size(); ++i) {
    if(s[i]==' ' || s[i]=='\0') {
      if(std::memcmp(s+prev,"NONE",i-prev)==0) {
        bits |= 0; // no effect
        }
      else if(std::memcmp(s+prev,"TARGET",i-prev)==0) {
        bits |= Trajectory::Target;
        }
      else if(std::memcmp(s+prev,"LINE",i-prev)==0) {
        bits |= Trajectory::Line;
        }
      else if(std::memcmp(s+prev,"SPLINE",i-prev)==0) {
        bits |= Trajectory::Spline;
        }
      else if(std::memcmp(s+prev,"RANDOM",i-prev)==0) {
        bits |= Trajectory::Random;
        }
      else if(std::memcmp(s+prev,"FIXED",i-prev)==0) {
        bits |= Trajectory::Fixed;
        }
      else if(std::memcmp(s+prev,"FOLLOW",i-prev)==0) {
        bits |= Trajectory::Follow;
        }
      else {
        if(i!=prev)
          Log::d("unknown trajectory flag: \"",s+prev,"\"");
        }
      prev = i+1;
      }
    }
  return Trajectory(bits);
  }

VisualFx::LoopMode VisualFx::loadLoopmode(const Daedalus::ZString& str) {
  if(str=="NONE")
    return LoopMode::LoopModeNone;
  if(str=="PINGPONG")
    return LoopMode::PinPong;
  if(str=="PINGPONG_ONCE")
    return LoopMode::PinPongOnce;
  if(str=="HALT")
    return LoopMode::Halt;
  return LoopMode::LoopModeNone;
  }

VisualFx::EaseFunc VisualFx::loadEaseFunc(const Daedalus::ZString& str) {
  if(str=="LINEAR")
    return EaseFunc::Linear;
  return EaseFunc::Linear;
  }

VisualFx::CollisionAlign VisualFx::loadCollisionAlign(const Daedalus::ZString& str) {
  if(str=="COLLISIONNORMAL")
    return CollisionAlign::Normal;
  if(str=="TRAJECTORY")
    return CollisionAlign::Trajectory;
  return CollisionAlign::Normal;
  }

VisualFx::Collision VisualFx::strToColision(std::string_view sv) {
  uint8_t bits = 0;
  while(sv.size()>0) {
    auto sp   = sv.find(' ');
    auto word = sv.substr(0,sp);
    if(word=="COLLIDE") {
      bits |= Collision::Collide;
      }
    else if(word=="CREATE") {
      bits |= Collision::Create;
      }
    else if(word=="CREATEONCE") {
      bits |= Collision::CreateOnce;
      }
    else if(word=="NORESP") {
      bits |= Collision::NoResp;
      }
    else if(word=="NORESP") {
      bits |= Collision::NoResp;
      }
    else if(word=="CREATEQUAD") {
      bits |= Collision::CreateQuad;
      }
    else {
      char buf[256] = {};
      std::snprintf(buf,sizeof(buf),"%*.s",int(word.size()),word.data());
      Log::d("unknown collision flag: \"",buf,"\"");
      }
    if(sp==std::string_view::npos)
      break;
    sv = sv.substr(sp+1);
    }
  return Collision(bits);
  }
