#include "movetrigger.h"

#include <Tempest/Log>

#include "graphics/mesh/animmath.h"
#include "game/serialize.h"
#include "world/world.h"

using namespace Tempest;

MoveTrigger::MoveTrigger(Vob* parent, World& world, const zenkit::VMover& mover, Flags flags)
  :AbstractTrigger(parent,world,mover,flags) {
  moverKeyFrames  = mover.keyframes;
  behavior        = mover.behavior;
  sfxOpenStart    = mover.sfx_open_start;
  stayOpenTimeSec = mover.stay_open_time_sec;
  sfxOpenEnd      = mover.sfx_open_end;
  sfxCloseEnd     = mover.sfx_close_end;
  sfxMoving       = mover.sfx_transitioning;
  visualName      = mover.visual_name;

  if(mover.cd_dynamic || mover.cd_static) {
    auto mesh = Resources::loadMesh(mover.visual_name);
    if(mesh!=nullptr)
      physic = PhysicMesh(*mesh,*world.physic(),true);
    }

  const float speed = mover.speed;
  keyframes.resize(mover.keyframes.size());
  for(size_t i=0; i<mover.keyframes.size(); ++i) {
    auto& f0 = mover.keyframes[i];
    auto& f1 = mover.keyframes[(i+1)%mover.keyframes.size()];
    auto  dx = (f1.position.x-f0.position.x);
    auto  dy = (f1.position.y-f0.position.y);
    auto  dz = (f1.position.z-f0.position.z);

    //float angle     = float(std::acos(std::clamp(glm::dot(f1.rotation, f0.rotation), -1.f, 1.f))*180.0/M_PI);
    //float angle     = float((glm::yaw(f1.rotation) - glm::yaw(f0.rotation))*180.0/M_PI);

    float theta = 2.f*std::pow(glm::dot(f1.rotation, f0.rotation), 2.f) - 1.f;
    float angle = float(std::acos(std::clamp(theta, -1.f, 1.f))*180.0/M_PI);

    float positionA = Vec3(dx,dy,dz).length();
    float positionB = float(angle) * 1.f;

    uint64_t ticksA = speed>0.0001f ? uint64_t(positionA/speed) : 0;
    uint64_t ticksB = speed>0.0001f ? uint64_t(positionB/speed) : 0;
    if(speed==0.001f) {
      // ring door in Halls of Irdorath. Possibly 0.001 is some magic value.
      ticksB = 1000;
      }

    keyframes[i].ticks = std::max(ticksA, ticksB);
    }

  if(!mover.keyframes.empty()) {
    state = Idle;
    frame = 0; //uint32_t(mover.keyframes.size()-1);
    }
  auto tr = transform();
  if(frame<mover.keyframes.size())
    tr = mkMatrix(mover.keyframes[frame]);
  tr.inverse();
  pos0 = localTransform();
  pos0.mul(tr);

  invalidateView();
  MoveTrigger::moveEvent();
  }

void MoveTrigger::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(pos0,uint8_t(state),sAnim,frame);
  }

void MoveTrigger::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),sAnim,frame);
  if(state!=Idle) {
    invalidateView();
    enableTicks();
    }
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
  auto fr  = mix(moverKeyFrames[f0],moverKeyFrames[f1],alpha);
  auto mat = pos0;
  mat.mul(mkMatrix(fr));
  setLocalTransform(mat);
  }

void MoveTrigger::invalidateView() {
  if(isDynamic() && !view.isEmpty())
    return;

  if(state!=Idle || isDynamic())
    setView(world.addView(visualName)); else
    setView(world.addStaticView(visualName));
  view.setObjMatrix(transform());
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
  if(behavior==zenkit::MoverBehavior::TRIGGER_CONTROL)
    processTrigger(e,false);
  }

void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(evt.move.key<0 || moverKeyFrames.size()<size_t(evt.move.key))
    return;
  Log::e("TODO: MoveTrigger::onGotoMsg");
  }

void MoveTrigger::processTrigger(const TriggerEvent& e, bool onTrigger) {
  if(moverKeyFrames.size()==0)
    return;
  if(state!=Idle) {
    world.triggerEvent(e);
    return;
    }

  std::string_view snd = sfxOpenStart;
  switch(behavior) {
    case zenkit::MoverBehavior::TOGGLE: {
      if(frame==0) {
        state = Close;
        } else {
        state = Open;
        }
      break;
      }
    case zenkit::MoverBehavior::TRIGGER_CONTROL: {
      if(onTrigger)
        state = Close; else
        state = Open;
      break;
      }
    case zenkit::MoverBehavior::OPEN_TIME: {
      state = Open;
      break;
      }
    case zenkit::MoverBehavior::LOOP: {
      state = Loop;
      break;
      }
    case zenkit::MoverBehavior::SINGLE_KEYS: {
      state = NextKey;
      break;
      }
    }

  sAnim = world.tickCount();
  enableTicks();
  // override view
  invalidateView();
  emitSound(snd);
  }

void MoveTrigger::tick(uint64_t /*dt*/) {
  if(state==Idle || keyframes.size()==0)
    return;

  uint32_t keySz = uint32_t(moverKeyFrames.size());
  uint32_t maxFr = uint32_t(moverKeyFrames.size()-1);

  uint64_t maxTicks = 0;
  for(size_t i=0; ; ++i) {
    if(i>=keyframes.size())
      break;
    if(i+1>=keyframes.size() && state!=Loop && state!=NextKey)
      break;
    maxTicks += keyframes[i].ticks;
    }

  uint64_t dt = world.tickCount()-sAnim, ticks = 0;
  float    a  = 0;
  uint32_t f0 = 0, f1 = 0;

  bool finished = false;
  switch(state) {
    case Idle:
      break;
    case OpenTimed: {
      if(sAnim+uint64_t(stayOpenTimeSec*1000)<world.tickCount()) {
        state = Close;
        sAnim = world.tickCount();
        }
      return;
      }
    case Close:{
      dt = dt<maxTicks ? dt : maxTicks;
      for(f0=0; f0+1<keyframes.size(); ++f0) {
        if(dt<keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1       = std::min(f0+1,maxFr);
      ticks    = keyframes[f0].ticks;
      a        = ticks>0 ? float(dt)/float(ticks) : 1.f;
      finished = f0==maxFr;
      break;
      }
    case Open: {
      dt = dt<maxTicks ? maxTicks - dt : 0;
      for(f0=0; f0+1<keyframes.size(); ++f0) {
        if(dt<=keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1       = std::min(f0+1,maxFr);
      ticks    = keyframes[f0].ticks;
      a        = ticks>0 ? float(dt)/float(ticks) : 1.f;
      finished = (f0==0 && dt==0);
      break;
      }
    case Loop: {
      if(maxTicks>0)
        dt %= maxTicks;
      for(f0=0; f0+1<keyframes.size(); ++f0) {
        if(dt<=keyframes[f0].ticks)
          break;
        dt -= keyframes[f0].ticks;
        }
      f1       = uint32_t(f0+1)%keySz;
      ticks    = keyframes[f0].ticks;
      a        = ticks>0 ? float(dt)/float(ticks) : 1.f;
      break;
      }
    case NextKey: {
      f0       = frame;
      f1       = uint32_t(f0+1)%uint32_t(moverKeyFrames.size());
      ticks    = keyframes[f0].ticks;
      a        = ticks>0 ? float(dt)/float(ticks) : 1.f;
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
    invalidateView();
    disableTicks();

    if(!target.empty()) {
      TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
      world.triggerEvent(e);
      }

    std::string_view snd = sfxOpenEnd;
    if(prev==Close)
      snd = sfxCloseEnd;
    if(prev==NextKey)
      snd = "";
    if(behavior==zenkit::MoverBehavior::OPEN_TIME && prev==Open) {
      state = OpenTimed;
      sAnim = world.tickCount();
      // override view
      invalidateView();
      enableTicks();
      }
    emitSound(snd);
    } else {
    emitSound(sfxMoving);
    }
  }
