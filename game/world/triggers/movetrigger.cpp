#include "movetrigger.h"

#include <Tempest/Log>

#include "graphics/mesh/animmath.h"
#include "game/serialize.h"
#include "world/world.h"

using namespace Tempest;

MoveTrigger::MoveTrigger(Vob* parent, World& world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  auto& mover = data.zCMover;
  setView(world.addView(data.visual));
  if(data.cdDyn || data.cdStatic) {
    auto mesh = Resources::loadMesh(data.visual);
    if(mesh!=nullptr)
      physic = PhysicMesh(*mesh,*world.physic(),true);
    }
  if(mover.moverLocked && mover.keyframes.size()>0) {
    frame = uint32_t(mover.keyframes.size()-1);
    }
  auto tr = transform();
  if(frame<mover.keyframes.size())
    tr = mkMatrix(mover.keyframes[frame]);
  tr.inverse();
  pos0 = localTransform();
  pos0.mul(tr);

  keyframes.resize(mover.keyframes.size());
  for(size_t i=0; i<mover.keyframes.size(); ++i) {
    auto& f0 = mover.keyframes[i];
    auto& f1 = mover.keyframes[(i+1)%mover.keyframes.size()];
    auto  dx = (f1.position.x-f0.position.x);
    auto  dy = (f1.position.y-f0.position.y);
    auto  dz = (f1.position.z-f0.position.z);
    keyframes[i].position = Vec3(dx,dy,dz).length();
    keyframes[i].ticks    = uint64_t(keyframes[i].position/mover.moveSpeed);
    if(keyframes[i].ticks==0) {
      keyframes[i].ticks = uint64_t(1.f/mover.moveSpeed);
      if(mover.moverBehavior==ZenLoad::MoverBehavior::NSTATE_LOOP)
        keyframes[i].ticks = 10000; // HACK: windmil
      }
    }

  MoveTrigger::moveEvent();
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
  if(state!=Idle)
    enableTicks();
  }

bool MoveTrigger::hasVolume() const {
  return false;
  }

void MoveTrigger::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::emitSound(std::string_view snd,bool freeSlot) {
  auto p   = position();
  auto sfx = ::Sound(world,::Sound::T_Regular,snd,p,0,freeSlot);
  sfx.play();
  }

void MoveTrigger::advanceAnim(uint32_t f0, uint32_t f1, float alpha) {
  alpha    = std::max(0.f,std::min(alpha,1.f));
  auto fr  = mix(data.zCMover.keyframes[f0],data.zCMover.keyframes[f1],alpha);
  auto mat = pos0;
  mat.mul(mkMatrix(fr));
  setLocalTransform(mat);
  }

float MoveTrigger::pathLength() const {
  float len = 0;
  for(size_t i=0; i<keyframes.size(); ++i)
    len += keyframes[i].position;
  return len;
  }

void MoveTrigger::moveEvent() {
  AbstractTrigger::moveEvent();
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

void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(evt.move.key<0 || data.zCMover.keyframes.size()<size_t(evt.move.key))
    return;
  Log::e("TODO: MoveTrigger::onGotoMsg");
  }

void MoveTrigger::processTrigger(const TriggerEvent& e, bool onTrigger) {
  if(data.zCMover.keyframes.size()==0 || state!=Idle) {
    world.triggerEvent(e);
    return;
    }

  std::string_view snd = data.zCMover.sfxOpenStart;
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
  if(state==Idle || keyframes.size()==0)
    return;

  auto&    mover = data.zCMover;
  uint32_t keySz = uint32_t(mover.keyframes.size());
  uint32_t maxFr = uint32_t(mover.keyframes.size()-1);

  uint64_t maxTicks = 0;
  for(size_t i=0; ; ++i) {
    if(i>=keyframes.size())
      break;
    if(i+1>=keyframes.size() && state!=Loop && state!=NextKey)
      break;
    maxTicks += keyframes[i].ticks;
    }

  uint64_t dt = world.tickCount()-sAnim;
  float    a  = 0;
  uint32_t f0 = 0, f1 = 0;

  bool finished = false;
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
      dt = dt<maxTicks ? dt : maxTicks;
      for(f0=0; f0<keyframes.size(); ++f0) {
        if(dt<keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1       = std::min(f0+1,maxFr);
      a        = float(dt)/float(keyframes[f0].ticks);
      finished = f0==maxFr;
      break;
      }
    case Close: {
      dt = dt<maxTicks ? maxTicks - dt : 0;
      for(f0=0; f0<keyframes.size(); ++f0) {
        if(dt<keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1       = std::min(f0+1,maxFr);
      a        = float(dt)/float(keyframes[f0].ticks);
      finished = (f0==0 && dt==0);
      break;
      }
    case Loop: {
      if(maxTicks>0)
        dt %= maxTicks;
      for(f0=0; f0<keyframes.size(); ++f0) {
        if(dt<=keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1 = uint32_t(f0+1)%keySz;
      a  = float(dt)/float(keyframes[f0].ticks);
      break;
      }
    case NextKey: {
      f0       = frame;
      f1       = uint32_t(f0+1)%uint32_t(mover.keyframes.size());
      a        = float(dt)/float(keyframes[f0].ticks);
      a        = std::min(1.f,a);
      finished = dt>keyframes[f0].ticks;
      break;
      }
    }

  advanceAnim(f0,f1,a);

  if(finished) {
    auto prev = state;
    state = Idle;
    frame = f0;
    disableTicks();

    if(data.zCTrigger.triggerTarget.size()>0) {
      TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName,TriggerEvent::T_Activate);
      world.triggerEvent(e);
      }

    std::string_view snd = data.zCMover.sfxOpenEnd;
    if(prev==Close)
      snd = data.zCMover.sfxCloseEnd;
    if(prev==NextKey)
      snd = "";
    if(data.zCMover.moverBehavior==ZenLoad::MoverBehavior::STATE_OPEN_TIMED && prev==Open) {
      state = OpenTimed;
      sAnim = world.tickCount();
      enableTicks();
      }
    emitSound(snd);
    } else {
    emitSound(data.zCMover.sfxMoving);
    }
  }
