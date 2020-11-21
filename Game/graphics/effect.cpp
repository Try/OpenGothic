#include "effect.h"

#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "world/world.h"
#include "world/npc.h"
#include "visualfx.h"

using namespace Tempest;

Effect::Effect(PfxObjects::Emitter&& visual, const char* node)
  :visual(std::move(visual)), nodeSlot(node){
  }

Effect::Effect(const VisualFx& vfx, World& owner, const Npc& src, SpellFxKey key)
  :Effect(vfx,owner,src.position() + Vec3(0,src.translateY(),0), key){
  }

Effect::Effect(const VisualFx& v, World& owner, const Vec3& inPos, SpellFxKey k) {
  root     = &v;
  nodeSlot = root->origin();
  visual   = root->visual(owner);

  auto& h  = root->handle();
  owner.emitSoundEffect(h.sfxID.c_str(), inPos.x,inPos.y,inPos.z,25,true);
  if(!h.emFXCreate_S.empty()) {
    auto vfx = owner.script().getVisualFx(h.emFXCreate_S.c_str());
    if(vfx!=nullptr)
      next.reset(new Effect(*vfx,owner,inPos,k));
    }

  if(k==SpellFxKey::Count || root==nullptr) {
    setPosition(inPos);
    } else {
    pos.identity();
    pos.translate(inPos);
    setKey(owner,k);
    }
  }

void Effect::setActive(bool e) {
  if(next!=nullptr)
    next->setActive(e);
  visual.setActive(e);
  }

void Effect::setLooped(bool l) {
  if(next!=nullptr)
    next->setLooped(l);
  visual.setLooped(l);
  }

void Effect::setTarget(const Tempest::Vec3& tg) {
  if(next!=nullptr)
    next->setTarget(tg);
  visual.setTarget(tg);
  }

void Effect::setObjMatrix(Tempest::Matrix4x4& mt) {
  syncAttaches(mt,true);
  }

void Effect::setPosition(const Vec3& pos3) {
  if(next!=nullptr)
    next->setPosition(pos3);

  pos.set(3,0, pos3.x);
  pos.set(3,1, pos3.y);
  pos.set(3,2, pos3.z);
  syncAttachesSingle(pos,true);
  }

void Effect::syncAttaches(const Matrix4x4& inPos, bool topLevel) {
  if(next!=nullptr)
    next->syncAttaches(inPos,false);
  syncAttachesSingle(inPos,topLevel);
  }

void Effect::syncAttachesSingle(const Matrix4x4& inPos, bool topLevel) {
  pos    = inPos;
  auto p = inPos;

  if(pose!=nullptr && boneId<pose->transform().size())
    p.mul(pose->transform(boneId));

  const float emTrjEaseVel = root==nullptr ? 0.f : root->handle().emTrjTargetElev;
  p.set(3,1, p.at(3,1)+emTrjEaseVel);
  Vec3 pos3 = {p.at(3,0),p.at(3,1),p.at(3,2)};

  if(topLevel) // HACK: VOB_MAGICBURN
    visual.setObjMatrix(p); else
    visual.setPosition(pos3);
  light.setPosition(pos3);
  }

void Effect::setKey(World& owner, SpellFxKey k) {
  if(k==SpellFxKey::Count)
    return;
  if(next!=nullptr)
    next->setKey(owner,k);

  if(root==nullptr)
    return;

  Vec3 pos3 = {pos.at(3,0),pos.at(3,1),pos.at(3,2)};
  key       = &root->key(k);

  auto vfx = owner.script().getVisualFx(key->emCreateFXID.c_str());
  if(vfx!=nullptr) {
    auto ex = Effect(*vfx,owner,pos3,SpellFxKey::Count);
    if(skeleton!=nullptr && pose!=nullptr) {
      size_t nid = 0;//skeleton->findNode(vfx->origin());
      if(nid<pose->transform().size()) {
        Matrix4x4 p = pos;
        p.mul(pose->transform(nid));
        ex.setObjMatrix(p);
        ex.setTarget(pos3);
        }
      }
    ex.setActive(true);
    owner.runEffect(std::move(ex));
    }

  const ParticleFx* pfx = owner.script().getParticleFx(*key);
  //const ParticleFx* pfx = owner.script().getParticleFx(key->visName_S.c_str());
  if(pfx!=nullptr) {
    visual = owner.getView(pfx);
    visual.setActive(true);
    }

  if(key->lightRange>0.0) {
    light = owner.getLight();
    light.setColor(Vec3(1,1,1));
    light.setRange(key->lightRange);
    const Daedalus::ZString* preset = &root->handle().lightPresetName;
    if(!key->lightPresetName.empty())
      preset = &key->lightPresetName;
    switch(toPreset(*preset)) {
      case NoPreset:
        light = LightGroup::Light();
        break;
      case JUSTWHITE:
      case WHITEBLEND:
        light.setColor(Vec3(1,1,1));
        break;
      case AURA:
        light.setColor(Vec3(0,0.5,1));
        break;
      case REDAMBIENCE:
        light.setColor(Vec3(1,0,0));
        break;
      case FIRESMALL:
        light.setColor(Vec3(1,1,0));
        break;
      case CATACLYSM:
        light.setColor(Vec3(1,0,0));
        break;
      }
    } else {
    light = LightGroup::Light();
    }

  setObjMatrix(pos);
  owner.emitSoundEffect(key->sfxID.c_str(),pos3.x,pos3.y,pos3.z,25,true);
  }

uint64_t Effect::effectPrefferedTime() const {
  uint64_t ret = next==nullptr ? 0 : next->effectPrefferedTime();
  if(root!=nullptr) {
    float timeF = root->handle().emFXLifeSpan;
    if(timeF>0)
      return std::max(ret,uint64_t(timeF*1000.f));
    }
  return std::max(ret,visual.effectPrefferedTime());
  }

void Effect::bindAttaches(const Pose& p, const Skeleton& to) {
  if(next!=nullptr)
    next->bindAttaches(p,to);

  skeleton = &to;
  pose     = &p;
  boneId   = to.findNode(nodeSlot);
  }

void Effect::onCollide(World& owner, const Vec3& pos, Npc* npc) {
  if(root==nullptr)
    return;

  const char* fxName = root->colStat();
  if(npc!=nullptr)
    fxName = root->colDyn();

  const VisualFx* vfx = owner.script().getVisualFx(fxName);
  if(vfx!=nullptr) {
    Effect eff(*vfx,owner,pos,SpellFxKey::Count);
    eff.setActive(true);
    owner.runEffect(std::move(eff));
    }

  vfx = owner.script().getVisualFx(root->handle().emFXCollDynPerc_S.c_str());
  if(vfx!=nullptr && npc!=nullptr) {
    npc->playEffect(*npc,*vfx);
    }
  }

Effect::LightPreset Effect::toPreset(const Daedalus::ZString& str) {
  if(str=="JUSTWHITE")
    return JUSTWHITE;
  if(str=="WHITEBLEND")
    return WHITEBLEND;
  if(str=="AURA")
    return AURA;
  if(str=="REDAMBIENCE")
    return REDAMBIENCE;
  if(str=="FIRESMALL")
    return FIRESMALL;
  if(str=="CATACLYSM")
    return CATACLYSM;
  Log::e("unknown light preset: \"",str.c_str(),"\"");
  return NoPreset;
  }
