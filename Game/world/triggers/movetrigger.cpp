#include "movetrigger.h"

#include "world/world.h"
#include "graphics/animmath.h"

MoveTrigger::MoveTrigger(Vob* parent, World& world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  setView(world.getView(data.visual.c_str()));
  if(data.cdDyn || data.cdStatic) {
    auto mesh = Resources::loadMesh(data.visual);
    physic    = PhysicMesh(*mesh,*world.physic());
    }
  auto tr = transform();
  tr.inverse();
  pos0 = tr;
  pos0.mul(localTransform());
  moveEvent();
  }

bool MoveTrigger::hasVolume() const {
  return false;
  }

void MoveTrigger::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::emitSound(const char* snd,bool freeSlot) {
  auto p = position();
  world.emitSoundEffect(snd,p.x,p.y,p.z,0,freeSlot);
  }

void MoveTrigger::moveEvent() {
  view  .setObjMatrix(transform());
  physic.setObjMatrix(transform());
  }

void MoveTrigger::onTrigger(const TriggerEvent&) {
  if(data.zCMover.keyframes.size()==0)
    return;
  if(anim!=IdleClosed && anim!=IdleOpenned)
    return;

  if(anim==IdleClosed)
    anim = Open; else
    anim = Close;
  sAnim = world.tickCount();

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

  uint64_t dt = world.tickCount()-sAnim;
  float    a  = float(dt%frameTicks)/float(frameTicks);
  uint32_t f0 = 0, f1 = 0;

  if(data.zCMover.moverBehavior==ZenLoad::MoverBehavior::NSTATE_LOOP) {
    size_t keySz = data.zCMover.keyframes.size();
    if(keySz>0) {
      f0 = uint32_t(dt/frameTicks)%uint32_t(keySz);
      f1 = uint32_t(f0+1         )%uint32_t(keySz);
      }
    } else
  if(anim==Open) {
    f0 = std::min(uint32_t(dt/frameTicks),maxFr);
    f1 = std::min(f0+1,maxFr);
    } else
  if(anim==Close) {
    f0 = maxFr - std::min(maxFr,uint32_t(dt/frameTicks));
    f1 = f0>0 ? f0-1 : 0;
    }

  auto fr  = mix(data.zCMover.keyframes[f0],data.zCMover.keyframes[f1],a);
  auto mat = pos0;
  mat.mul(mkMatrix(fr));
  setLocalTransform(mat);

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
