#include "effect.h"

#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "visualfx.h"

using namespace Tempest;

Effect::Effect(PfxEmitter&& pfx, const char* node)
  :pfx(std::move(pfx)), nodeSlot(node) {
  pos.identity();
  }

Effect::Effect(const VisualFx& vfx, World& owner, const Npc& src, SpellFxKey key)
  :Effect(vfx,owner,src.position(), key) {
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
    gfx = owner.addGlobalEffect(h.visName_S,h.emFXLifeSpan,h.userString,Daedalus::GEngineClasses::VFX_NUM_USERSTRINGS);
    } else {
    pfx = root->visual(owner);
    if(root->isMeshEmmiter())
      pfx.setMesh(meshEmitter,pose);
    }
  pos.identity();

  if(!h.emFXCreate_S.empty()) {
    auto vfx = owner.script().getVisualFx(h.emFXCreate_S.c_str());
    if(vfx!=nullptr)
      next.reset(new Effect(*vfx,owner,inPos,k));
    }

  if(k==SpellFxKey::Count) {
    setupLight(owner,nullptr);
    setPosition(inPos);
    } else {
    pos.identity();
    pos.translate(inPos);
    setKey(owner,k);
    }

  if(sfx.isEmpty()) {
    sfx = ::Sound(owner,::Sound::T_Regular,h.sfxID.c_str(),inPos,2500.f,false);
    sfx.setAmbient(h.sfxIsAmbient);
    sfx.play();
    }
  }

Effect::~Effect() {
  sfx.setLooping(false);
  }

void Effect::setupLight(World& owner, const Effect::Key* key) {
  auto& hEff            = root->handle();
  auto* lightPresetName = &hEff.lightPresetName;
  if(key!=nullptr) {
    if(!key->lightPresetName.empty())
      lightPresetName = &key->lightPresetName;
    }

  if(lightPresetName->empty()) {
    light = LightGroup::Light();
    return;
    }

  light = LightGroup::Light(owner,lightPresetName->c_str());
  if(key!=nullptr)
    light.setRange(key->lightRange);
  }

bool Effect::is(const VisualFx& vfx) const {
  return root==&vfx;
  }

void Effect::setActive(bool e) {
  if(next!=nullptr)
    next->setActive(e);
  pfx.setActive(e);
  sfx.setActive(e);
  }

void Effect::setLooped(bool l) {
  if(looped==l)
    return;
  looped = l;
  if(next!=nullptr)
    next->setLooped(l);
  pfx.setLooped(l);
  }

void Effect::setTarget(const Tempest::Vec3& tg) {
  if(next!=nullptr)
    next->setTarget(tg);
  pfx.setTarget(tg);
  }

void Effect::setObjMatrix(Tempest::Matrix4x4& mt) {
  syncAttaches(mt);
  }

void Effect::setPosition(const Vec3& pos3) {
  if(next!=nullptr)
    next->setPosition(pos3);

  pos.set(3,0, pos3.x);
  pos.set(3,1, pos3.y);
  pos.set(3,2, pos3.z);
  syncAttachesSingle(pos);
  }

void Effect::syncAttaches(const Matrix4x4& inPos) {
  if(next!=nullptr)
    next->syncAttaches(inPos);
  syncAttachesSingle(inPos);
  }

void Effect::syncAttachesSingle(const Matrix4x4& inPos) {
  if(root==nullptr && pfx.isEmpty())
    return;

  pos = inPos;
  auto p = inPos;
  if(pose!=nullptr && boneId<pose->transform().size())
    p.mul(pose->transform(boneId));

  const float emTrjEaseVel = root==nullptr ? 0.f : root->handle().emTrjTargetElev;
  p.set(3,1, p.at(3,1)+emTrjEaseVel);
  Vec3 pos3 = {p.at(3,0),p.at(3,1),p.at(3,2)};

  pfx  .setObjMatrix(p);
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
    ex.setActive(true);
    if(pose!=nullptr && skeleton!=nullptr)
      ex.bindAttaches(*pose,*skeleton);
    next.reset(new Effect(std::move(ex)));
    }

  auto pfxDecl = owner.script().getParticleFx(*key);
  if(pfxDecl!=nullptr) {
    pfx = PfxEmitter(owner,pfxDecl);
    if(root->isMeshEmmiter())
      pfx.setMesh(meshEmitter,pose);
    pfx.setActive(true);
    pfx.setLooped(looped);
    }

  setupLight(owner,key);
  syncAttachesSingle(pos);
  sfx = ::Sound(owner,::Sound::T_Regular,key->sfxID.c_str(),pos3,2500.f,false);
  sfx.setAmbient(key->sfxIsAmbient);
  sfx.play();
  }

void Effect::setMesh(const MeshObjects::Mesh* mesh) {
  meshEmitter = mesh;
  if(root!=nullptr && root->isMeshEmmiter())
    pfx.setMesh(meshEmitter,pose);
  }

uint64_t Effect::effectPrefferedTime() const {
  uint64_t ret = next==nullptr ? 0 : next->effectPrefferedTime();
  if(ret==uint64_t(-1))
    ret = 0;
  if(root!=nullptr) {
    float timeF = root->handle().emFXLifeSpan;
    if(timeF>0)
      return std::max(ret,uint64_t(timeF*1000.f));
    if(timeF<0 && pfx.isEmpty())
      return uint64_t(-1);
    }
  ret = std::max(ret,pfx  .effectPrefferedTime());
  ret = std::max(ret,light.effectPrefferedTime());
  return ret;
  }

bool Effect::isAlive() const {
  if(next!=nullptr && next->isAlive())
    return true;
  return pfx.isAlive();
  }

void Effect::bindAttaches(const Pose& p, const Skeleton& to) {
  if(next!=nullptr)
    next->bindAttaches(p,to);

  skeleton = &to;
  pose     = &p;
  boneId   = to.findNode(nodeSlot);
  if(root!=nullptr && root->isMeshEmmiter())
    pfx.setMesh(meshEmitter,pose);
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
