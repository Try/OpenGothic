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

Effect::Effect(const VisualFx& v, World& owner, const Vec3& pos, SpellFxKey k) {
  root     = &v;
  nodeSlot = root->origin();
  visual   = root->visual(owner);

  auto& h  = root->handle();
  owner.emitSoundEffect(h.sfxID.c_str(), pos.x,pos.y,pos.z,25,true);
  if(!h.emFXCreate_S.empty()) {
    auto vfx = owner.script().getVisualFx(h.emFXCreate_S.c_str());
    if(vfx!=nullptr)
      next.reset(new Effect(*vfx,owner,pos,k));
    }

  if(k==SpellFxKey::Count || root==nullptr)
    setPosition(pos); else
    setKey(owner,pos,k);
  }

void Effect::setActive(bool e) {
  if(next!=nullptr)
    next->setActive(e);
  visual.setActive(e);
  }

void Effect::setTarget(const Tempest::Vec3& tg) {
  if(next!=nullptr)
    next->setTarget(tg);
  visual.setTarget(tg);
  }

void Effect::setObjMatrix(Tempest::Matrix4x4& mt) {
  if(next!=nullptr)
    next->setObjMatrix(mt);
  visual.setObjMatrix(mt);

  Vec3 pos = {mt.at(3,0),mt.at(3,1),mt.at(3,2)};
  light.setPosition(pos);
  }

void Effect::setPosition(const Vec3& pos) {
  if(next!=nullptr)
    next->setPosition(pos);

  const float emTrjEaseVel = root==nullptr ? 0.f : root->handle().emTrjTargetElev;
  Vec3 pos3 = {pos.x,pos.y+emTrjEaseVel,pos.z};

  visual.setPosition(pos3);
  light .setPosition(pos3);
  }

void Effect::setKey(World& owner, const Vec3& pos, SpellFxKey k) {
  if(k==SpellFxKey::Count)
    return;
  if(next!=nullptr)
    next->setKey(owner,pos,k);

  if(root==nullptr)
    return;

  key = &root->key(k);
  auto vfx = owner.script().getVisualFx(key->emCreateFXID.c_str());
  if(vfx!=nullptr) {
    // TODO: start new effect in parallel
    // visual = vfx->visual(owner);
    }
  const ParticleFx* pfx = owner.script().getParticleFx(key->visName_S.c_str());
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

  setPosition(pos);
  owner.emitSoundEffect(key->sfxID.c_str(),pos.x,pos.y,pos.z,25,true);
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

void Effect::bindAttaches(const Skeleton& to) {
  if(next!=nullptr)
    next->bindAttaches(to);
  boneId = to.findNode(nodeSlot);
  }

void Effect::rebindAttaches(const Skeleton& from, const Skeleton& to) {
  if(next!=nullptr)
    next->rebindAttaches(from,to);

  if(boneId<from.nodes.size()) {
    size_t nid = 0;
    if(nodeSlot==nullptr)
      nid = to.findNode(from.nodes[boneId].name); else
      nid = to.findNode(nodeSlot);
    boneId = nid;
    }
  }

void Effect::syncAttaches(const Pose& pose, const Matrix4x4& pos) {
  if(next!=nullptr)
    next->syncAttaches(pose,pos);

  auto p = pos;
  if(boneId<pose.transform().size())
    p.mul(pose.transform(boneId));

  const float emTrjEaseVel = root==nullptr ? 0.f : root->handle().emTrjTargetElev;
  p.set(3,1, p.at(3,1)+emTrjEaseVel);
  Vec3 pos3 = {p.at(3,0),p.at(3,1),p.at(3,2)};

  visual.setObjMatrix(p);
  light .setPosition(pos3);
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
