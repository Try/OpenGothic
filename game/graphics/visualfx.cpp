#include "visualfx.h"

#include <Tempest/Log>
#include <cctype>

#include "world/world.h"
#include "utils/parser.h"

using namespace Tempest;

VisualFx::VisualFx(Daedalus::GEngineClasses::CFx_Base &&fx, Daedalus::DaedalusVM& vm, const char* name) {
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

  emFXCreate               = nullptr;
  emFXInvestOrigin         = fx.emFXInvestOrigin_S.c_str();
  emFXInvestTarget         = fx.emFXInvestTarget_S.c_str();
  emFXTriggerDelay         = fx.emFXTriggerDelay<0 ? 0 : uint64_t(fx.emFXTriggerDelay*1000.f);
  emFXCreatedOwnTrj        = fx.emFXCreatedOwnTrj!=0;
  emActionCollDyn          = strToColision(fx.emActionCollStat_S.c_str());		// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE
  emActionCollStat         = strToColision(fx.emActionCollDyn_S.c_str());			// CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE, CREATEQUAD
  emFXCollStat             = nullptr;
  emFXCollDyn              = nullptr;
  emFXCollDynPerc          = nullptr;
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
  sfxID                    = fx.sfxID.c_str();
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

  for(int i=0;i<int(SpellFxKey::Count);++i) {
    char kname[256]={};
    std::snprintf(kname,sizeof(kname),"%s_KEY_%s",name,keyName[i]);
    auto id = vm.getDATFile().getSymbolIndexByName(kname);
    if(id==size_t(-1))
      continue;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey key;
    vm.initializeInstance(key, id, Daedalus::IC_FXEmitKey);
    vm.clearReferences(Daedalus::IC_FXEmitKey);
    keys[i] = key;
    }

  for(int i=1; ; ++i) {
    char kname[256]={};
    std::snprintf(kname,sizeof(kname),"%s_KEY_INVEST_%d",name,i);
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

const Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type, int32_t keyLvl) const {
  if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return investKeys[size_t(keyLvl)];
    }
  return keys[int(type)];
  }

VisualFx::Trajectory VisualFx::loadTrajectory(const Daedalus::ZString& str) {
  if(str=="NONE")
    return Trajectory::None;
  if(str=="TARGET")
    return Trajectory::Target;
  if(str=="LINE")
    return Trajectory::Line;
  if(str=="SPLINE")
    return Trajectory::Spline;
  if(str=="RANDOM")
    return Trajectory::Random;
  return Trajectory::None;
  }

VisualFx::LoopMode VisualFx::loadLoopmode(const Daedalus::ZString& str) {
  if(str=="NONE")
    return LoopMode::None;
  if(str=="PINGPONG")
    return LoopMode::PinPong;
  if(str=="PINGPONG_ONCE")
    return LoopMode::PinPongOnce;
  if(str=="HALT")
    return LoopMode::Halt;
  return LoopMode::None;
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

VisualFx::Collision VisualFx::strToColision(const char* s) {
  uint8_t bits = 0;
  size_t  prev = 0;
  for(size_t i=0; ; ++i) {
    if(s[i]==' ' || s[i]=='\0') {
      if(std::memcmp(s+prev,"COLLIDE",i-prev)==0) {
        bits |= Collision::Collide;
        }
      else if(std::memcmp(s+prev,"CREATE",i-prev)==0) {
        bits |= Collision::Create;
        }
      else if(std::memcmp(s+prev,"CREATEONCE",i-prev)==0) {
        bits |= Collision::CreateOnce;
        }
      else if(std::memcmp(s+prev,"NORESP",i-prev)==0) {
        bits |= Collision::NoResp;
        }
      else if(std::memcmp(s+prev,"CREATEQUAD",i-prev)==0) {
        bits |= Collision::CreateQuad;
        }
      else {
        if(i!=prev)
          Log::d("unknown collision flag: \"",s+prev,"\"");
        }
      prev = i+1;
      }
    if(s[i]=='\0')
      break;
    }
  return Collision(bits);
  }
