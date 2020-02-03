#include "movetrigger.h"

#include "world/world.h"
#include "graphics/animmath.h"

MoveTrigger::MoveTrigger(ZenLoad::zCVobData&& d,World& w)
  :AbstractTrigger(std::move(d),w) {
  float v[16]={};
  std::memcpy(v,data.worldMatrix.m,sizeof(v));

  setView(owner.getView(data.visual.c_str()));
  if(data.cdDyn || data.cdStatic) {
    auto mesh   = Resources::loadMesh(data.visual);
    auto physic = Resources::physicMesh(mesh);
    setPhysic(owner.physic()->staticObj(physic,Tempest::Matrix4x4(v)));
    }
  pos = Tempest::Matrix4x4(v);
  setObjMatrix(pos);
  }

bool MoveTrigger::hasVolume() const {
  return false;
  }

void MoveTrigger::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::setPhysic(DynamicWorld::StaticItem &&p) {
  physic = std::move(p);
  }

void MoveTrigger::setObjMatrix(const Tempest::Matrix4x4 &m) {
  view  .setObjMatrix(m);
  physic.setObjMatrix(m);
  }

void MoveTrigger::emitSound(const char* snd,bool freeSlot) {
  owner.emitSoundEffect(snd,pos.at(3,0),pos.at(3,1),pos.at(3,2),0,freeSlot);
  }

void MoveTrigger::onTrigger(const TriggerEvent&) {
  if(data.zCMover.keyframes.size()==0)
    return;
  if(anim!=IdleClosed && anim!=IdleOpenned)
    return;

  if(anim==IdleClosed)
    anim = Open; else
    anim = Close;

  sAnim = owner.tickCount();
  enableTicks();

  const char* snd = data.zCMover.sfxOpenStart.c_str();
  if(anim==Close)
    snd = data.zCMover.sfxCloseStart.c_str();
  emitSound(snd);
  }

void MoveTrigger::tick(uint64_t /*dt*/) {
  uint32_t maxFr      = uint32_t(data.zCMover.keyframes.size()-1);
  uint64_t frameTicks = uint64_t(1000/data.zCMover.moveSpeed);
  frameTicks/=10; //HACK
  if(frameTicks==0)
    frameTicks=1;

  uint64_t dt = owner.tickCount()-sAnim;
  float    a  = float(dt%frameTicks)/float(frameTicks);
  uint32_t f0 = 0, f1 = 0;

  if(anim==Open) {
    f0 = uint32_t(dt/frameTicks);
    f1 = std::min(f0+1,maxFr);
    } else {
    f0 = maxFr - std::min(maxFr,uint32_t(dt/frameTicks));
    f1 = f0>0 ? f0-1 : 0;
    }

  auto fr = mix(data.zCMover.keyframes[f0],data.zCMover.keyframes[f1],a);
  setObjMatrix(mkMatrix(fr));

  if(f0==f1) {
    if(anim==Close)
      anim = IdleClosed; else
      anim = IdleOpenned;
    disableTicks();

    const char* snd = data.zCMover.sfxOpenEnd.c_str();
    if(anim==Close)
      snd = data.zCMover.sfxCloseEnd.c_str();
    emitSound(snd);
    } else {
    emitSound(data.zCMover.sfxMoving.c_str());
    }
  }
