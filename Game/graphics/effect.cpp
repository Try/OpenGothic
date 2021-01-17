#include "effect.h"

#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "visualfx.h"

using namespace Tempest;

Effect::Effect(PfxEmitter&& visual, const char* node)
  :visual(std::move(visual)), nodeSlot(node) {
  pos.identity();
  }

Effect::Effect(const VisualFx& vfx, World& owner, const Npc& src, SpellFxKey key)
  :Effect(vfx,owner,src.position() + Vec3(0,src.translateY(),0), key) {
  pos.identity();
  }

Effect::Effect(const VisualFx& v, World& owner, const Vec3& inPos, SpellFxKey k) {
  root     = &v;
  nodeSlot = root->origin();
  auto& h  = root->handle();

  if(h.visName_S=="time.slw"  ||
     h.visName_S=="morph.fov" ||
     h.visName_S=="screenblend.scx" ||
     h.visName_S=="earthquake.eqk") {
    gfx    = owner.addGlobalEffect(h.visName_S,h.emFXLifeSpan,h.userString,Daedalus::GEngineClasses::VFX_NUM_USERSTRINGS);
    } else {
    visual = root->visual(owner);
    }
  pos.identity();


  if(!h.emFXCreate_S.empty()) {
    auto vfx = owner.script().getVisualFx(h.emFXCreate_S.c_str());
    if(vfx!=nullptr)
      next.reset(new Effect(*vfx,owner,inPos,k));
    }

  if(k==SpellFxKey::Count) {
    setPosition(inPos);
    } else {
    pos.identity();
    pos.translate(inPos);
    setKey(owner,k);
    }

  if(sfx.isEmpty()) {
    sfx = owner.addSoundEffect(h.sfxID.c_str(), inPos.x,inPos.y,inPos.z,25,false);
    sfx.play();
    sfx.setLooping(h.sfxIsAmbient);
    }
  }

Effect::~Effect() {
  sfx.setLooping(false);
  }

bool Effect::is(const VisualFx& vfx) const {
  return root==&vfx;
  }

void Effect::setActive(bool e) {
  if(next!=nullptr)
    next->setActive(e);
  visual.setActive(e);
  sfx   .setActive(e);
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

void Effect::setKey(World& owner, SpellFxKey k, int32_t keyLvl) {
  if(k==SpellFxKey::Count)
    return;
  if(next!=nullptr)
    next->setKey(owner,k);

  if(root==nullptr)
    return;

  Vec3 pos3 = {pos.at(3,0),pos.at(3,1),pos.at(3,2)};
  key       = &root->key(k,keyLvl);

  auto vfx = owner.script().getVisualFx(key->emCreateFXID.c_str());
  if(vfx!=nullptr) {
    auto ex = Effect(*vfx,owner,pos3,SpellFxKey::Count);
    if(skeleton!=nullptr && pose!=nullptr) {
      // size_t nid = 0;
      size_t nid = skeleton->findNode(vfx->origin());
      Matrix4x4 p = pos;
      if(nid<pose->transform().size()) {
        p.mul(pose->transform(nid));
        }
      pos3 = {p.at(3,0),p.at(3,1),p.at(3,2)};
      ex.setPosition(pos3);
      ex.setTarget  (pos3);
      }
    ex.setActive(true);
    owner.runEffect(std::move(ex));
    }

  const ParticleFx* pfx = owner.script().getParticleFx(*key);
  if(pfx!=nullptr) {
    visual = owner.addView(pfx);
    visual.setActive(true);
    }

  if(key->lightRange>0.0) {
    light = owner.addLight();
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
  sfx = owner.addSoundEffect(key->sfxID.c_str(),pos3.x,pos3.y,pos3.z,25,true);
  sfx.play();
  sfx.setLooping(key->sfxIsAmbient);
  }

uint64_t Effect::effectPrefferedTime() const {
  uint64_t ret = next==nullptr ? 0 : next->effectPrefferedTime();
  if(ret==uint64_t(-1))
    ret = 0;
  if(root!=nullptr) {
    float timeF = root->handle().emFXLifeSpan;
    if(timeF>0)
      return std::max(ret,uint64_t(timeF*1000.f));
    if(timeF<0)
      return uint64_t(-1);
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
    npc->startEffect(*npc,*vfx);
    }
  }

std::unique_ptr<Effect> Effect::takeNext() {
  return std::move(next);
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
