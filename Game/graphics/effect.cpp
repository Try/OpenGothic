#include "effect.h"

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
  }

void Effect::setKey(World& owner, const Vec3& pos, SpellFxKey k) {
  if(k==SpellFxKey::Count || root==nullptr)
    return;
  if(next!=nullptr)
    next->setKey(owner,pos,k);

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
  owner.emitSoundEffect(key->sfxID.c_str(),pos.x,pos.y,pos.z,25,true);
  }

uint64_t Effect::effectPrefferedTime() const {
  if(root!=nullptr) {
    float timeF = root->handle().emFXLifeSpan;
    if(timeF>0)
      return uint64_t(timeF*1000.f);
    }
  return visual.effectPrefferedTime();
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
  visual.setObjMatrix(p);
  }
