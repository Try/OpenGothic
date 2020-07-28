#include "movetrigger.h"

#include <Tempest/Log>

#include "world/world.h"
#include "graphics/animmath.h"
#include "game/serialize.h"

using namespace Tempest;

MoveTrigger::MoveTrigger(Vob* parent, World& world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  setView(world.getView(data.visual.c_str()));
  if(data.cdDyn || data.cdStatic) {
    auto mesh = Resources::loadMesh(data.visual);
    physic    = PhysicMesh(*mesh,*world.physic());
    }
  if(data.zCMover.moverLocked && data.zCMover.keyframes.size()>0) {
    frame = uint32_t(data.zCMover.keyframes.size()-1);
    }
  auto tr = transform();
  if(frame<data.zCMover.keyframes.size())
    tr = mkMatrix(data.zCMover.keyframes[frame]);
  tr.inverse();
  pos0 = localTransform();
  pos0.mul(tr);

  moveEvent();
  }

void MoveTrigger::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(pos0,uint8_t(state),sAnim,frame);
  }

void MoveTrigger::load(Serialize& fin) {
  if(fin.version()<10)
    return;
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),sAnim,frame);
  moveEvent();
  if(state!=Idle)
    enableTicks();
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

void MoveTrigger::advanceAnim(uint32_t f0, uint32_t f1, float alpha) {
  alpha    = std::max(0.f,std::min(alpha,1.f));
  auto fr  = mix(data.zCMover.keyframes[f0],data.zCMover.keyframes[f1],alpha);
  auto mat = pos0;
  mat.mul(mkMatrix(fr));
  setLocalTransform(mat);
  }

void MoveTrigger::moveEvent() {
  Vob::moveEvent();
  view  .setObjMatrix(transform());
  physic.setObjMatrix(transform());
  }

void MoveTrigger::onTrigger(const TriggerEvent& e) {
  processTrigger(e,true);
  }

void MoveTrigger::onUntrigger(const TriggerEvent& e) {
  if(data.zCMover.moverBehavior==ZenLoad::MoverBehavior::STATE_TRIGGER_CTRL)
     processTrigger(e,false);
  }

void MoveTrigger::processTrigger(const TriggerEvent& e, bool onTrigger) {
  if(data.zCMover.keyframes.size()==0 || state!=Idle) {
    world.triggerEvent(e);
    return;
    }

  const char* snd = data.zCMover.sfxOpenStart.c_str();
  switch(data.zCMover.moverBehavior) {
    case ZenLoad::MoverBehavior::STATE_TOGGLE: {
      if(frame+1==data.zCMover.keyframes.size()) {
        state = Close;
        } else {
        state = Open;
        }
      break;
      }
    case ZenLoad::MoverBehavior::STATE_TRIGGER_CTRL: {
      if(onTrigger)
        state = Open; else
        state = Close;
      break;
      }
    case ZenLoad::MoverBehavior::STATE_OPEN_TIMED: {
      state = Open;
      break;
      }
    case ZenLoad::MoverBehavior::NSTATE_LOOP: {
      state = Loop;
      break;
      }
    case ZenLoad::MoverBehavior::NSTATE_SINGLE_KEYS: {
      state = NextKey;
      break;
      }
    }

  sAnim = world.tickCount();
  enableTicks();
  emitSound(snd);
  }

void MoveTrigger::tick(uint64_t /*dt*/) {
  if(state==Idle)
    return;

  auto&    mover      = data.zCMover;
  uint32_t keySz      = uint32_t(mover.keyframes.size());
  uint32_t maxFr      = uint32_t(mover.keyframes.size()-1);
  uint64_t frameTicks = uint64_t(60.f/mover.moveSpeed);
  //if(mover.keyframes.size()>0)
  //  frameTicks/=mover.keyframes.size();
  if(frameTicks==0)
    frameTicks=1;
  if(data.zCMover.moverBehavior!=ZenLoad::MoverBehavior::NSTATE_LOOP) {
    // NOTE: winmill seem to rellay on mover.moveSpeed, but irdorath - not exactly
    if(frameTicks>1000)
      frameTicks=1000;
    }

  uint64_t dt = world.tickCount()-sAnim;
  float    a  = float(dt%frameTicks)/float(frameTicks);
  uint32_t f0 = 0, f1 = 0;

  switch(state) {
    case Idle:
      break;
    case OpenTimed: {
      if(sAnim+uint64_t(mover.stayOpenTimeSec*1000)<world.tickCount()) {
        state = Close;
        sAnim = world.tickCount();
        }
      return;
      }
    case Open:{
      f0 = std::min(uint32_t(dt/frameTicks),maxFr);
      f1 = std::min(f0+1,maxFr);
      a  = float(dt-f0*frameTicks)/float(frameTicks);
      break;
      }
    case Close: {
      uint32_t offset = std::min(uint32_t(dt/frameTicks),maxFr);
      f0 = maxFr-offset;
      f1 = f0>0 ? f0-1 : 0;
      a  = float(dt-offset*frameTicks)/float(frameTicks);
      break;
      }
    case Loop: {
      f0 = uint32_t(dt/frameTicks)%keySz;
      f1 = uint32_t(f0+1)%keySz;
      a  = float(dt%frameTicks)/float(frameTicks);
      break;
      }
    case NextKey: {
      f0 = frame;
      f1 = uint32_t(f0+1)%uint32_t(mover.keyframes.size());
      a  = std::min(1.f,float(dt)/float(frameTicks));
      break;
      }
    }

  advanceAnim(f0,f1,a);

  if(f0==f1 || (state==NextKey && dt>=frameTicks)) {
    auto prev = state;
    state = Idle;
    frame = f0;
    disableTicks();

    if(data.zCTrigger.triggerTarget.size()>0){
      TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName,TriggerEvent::T_Activate);
      world.triggerEvent(e);
      }

    const char* snd = data.zCMover.sfxOpenEnd.c_str();
    if(prev==Close)
      snd = data.zCMover.sfxCloseEnd.c_str();
    if(prev==NextKey)
      snd = nullptr;
    if(data.zCMover.moverBehavior==ZenLoad::MoverBehavior::STATE_OPEN_TIMED && prev==Open) {
      state = OpenTimed;
      sAnim = world.tickCount();
      enableTicks();
      }
    emitSound(snd);
    } else {
    emitSound(data.zCMover.sfxMoving.c_str());
    }
  }
