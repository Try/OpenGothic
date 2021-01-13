#include "visualfx.h"

#include <Tempest/Log>
#include <cctype>

#include "world/world.h"

using namespace Tempest;

VisualFx::VisualFx(Daedalus::GEngineClasses::CFx_Base &&src, Daedalus::DaedalusVM& vm, const char* name)
  :fx(std::move(src)) {
  emTrjOriginNode = fx.emTrjOriginNode.c_str();
  for(auto& c:emTrjOriginNode)
    c = char(std::toupper(c));
  colStatFlg = strToColision(fx.emActionCollStat_S.c_str());
  colDynFlg  = strToColision(fx.emActionCollDyn_S.c_str());  

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

const char* VisualFx::colStat() const {
  return fx.emFXCollStat_S.c_str();
  }

const char* VisualFx::colDyn() const {
  return fx.emFXCollDyn_S.c_str();
  }

PfxEmitter VisualFx::visual(World& owner) const {
  const ParticleFx* pfx = owner.script().getParticleFx(fx.visName_S.c_str());
  if(pfx==nullptr)
    return PfxEmitter();
  return owner.getView(pfx);
  }

const Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type, int32_t keyLvl) const {
  if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return investKeys[keyLvl];
    }
  return keys[int(type)];
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
