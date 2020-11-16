#include "visualfx.h"

#include <Tempest/Log>
#include <cctype>

#include "world/world.h"

using namespace Tempest;

VisualFx::VisualFx(Daedalus::GEngineClasses::CFx_Base &&src):fx(std::move(src)) {
  emTrjOriginNode = fx.emTrjOriginNode.c_str();
  for(auto& c:emTrjOriginNode)
    c = char(std::toupper(c));
  colStatFlg = strToColision(fx.emActionCollStat_S.c_str());
  colDynFlg  = strToColision(fx.emActionCollDyn_S.c_str());
  }

const char* VisualFx::colStat() const {
  return fx.emFXCollStat_S.c_str();
  }

const char* VisualFx::colDyn() const {
  return fx.emFXCollDyn_S.c_str();
  }

PfxObjects::Emitter VisualFx::visual(World& owner) const {
  const ParticleFx* pfx = owner.script().getParticleFx(fx.visName_S.c_str());
  if(pfx==nullptr)
    return PfxObjects::Emitter();
  auto vemitter = owner.getView(pfx);
  // vemitter.setObjMatrix();
  return vemitter;
  }

const Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type) const {
  return keys[int(type)];
  }

Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type) {
  return keys[int(type)];
  }

void VisualFx::emitSound(World& wrld, const Tempest::Vec3& p, SpellFxKey type) const {
  auto& k   = key(type);
  wrld.emitSoundEffect(k.sfxID.c_str(),p.x,p.y,p.z,0,true);
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
