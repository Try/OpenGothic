#include "movetrigger.h"

#include <Tempest/Log>

#include "graphics/mesh/animmath.h"
#include "game/serialize.h"
#include "world/world.h"

using namespace Tempest;

MoveTrigger::MoveTrigger(Vob* parent, World& world, const phoenix::vobs::trigger_mover& mover, Flags flags)
  :AbstractTrigger(parent,world,mover,flags) {
  mover_keyframes = mover.keyframes;
  behavior = mover.behavior;
  sfxOpenStart = mover.sfx_open_start;
  stayOpenTimeSec = mover.stay_open_time_sec;
  sfxOpenEnd = mover.sfx_open_end;
  sfxCloseEnd = mover.sfx_close_end;
  sfxMoving = mover.sfx_transitioning;
  visualName = mover.visual_name;

  if(mover.cd_dynamic || mover.cd_static) {
    auto mesh = Resources::loadMesh(mover.visual_name);
    if(mesh!=nullptr)
      physic = PhysicMesh(*mesh,*world.physic(),true);
    }
  if(mover.locked && !mover.keyframes.empty()) {
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
    keyframes[i].ticks    = uint64_t(keyframes[i].position/mover.speed);
    if(keyframes[i].ticks==0) {
      keyframes[i].ticks = uint64_t(1.f/mover.speed);
      if(mover.behavior==phoenix::mover_behavior::loop)
        keyframes[i].ticks = 10000; // HACK: windmil
      }
    }

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
  auto fr  = mix(mover_keyframes[f0],mover_keyframes[f1],alpha);
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
  if(behavior==phoenix::mover_behavior::trigger_control)
    processTrigger(e,false);
  }

void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(evt.move.key<0 || mover_keyframes.size()<size_t(evt.move.key))
    return;
  Log::e("TODO: MoveTrigger::onGotoMsg");
  }

void MoveTrigger::processTrigger(const TriggerEvent& e, bool onTrigger) {
  if(mover_keyframes.size()==0 || state!=Idle) {
    world.triggerEvent(e);
    return;
    }

  std::string_view snd = sfxOpenStart;
  switch(behavior) {
    case phoenix::mover_behavior::toggle: {
      if(frame+1==mover_keyframes.size()) {
        state = Close;
        } else {
        state = Open;
        }
      break;
      }
    case phoenix::mover_behavior::trigger_control: {
      if(onTrigger)
        state = Open; else
        state = Close;
      break;
      }
    case phoenix::mover_behavior::open_timed: {
      state = Open;
      break;
      }
    case phoenix::mover_behavior::loop: {
      state = Loop;
      break;
      }
    case phoenix::mover_behavior::single_keys: {
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

  uint32_t keySz = uint32_t(mover_keyframes.size());
  uint32_t maxFr = uint32_t(mover_keyframes.size()-1);

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
      if(sAnim+uint64_t(stayOpenTimeSec*1000)<world.tickCount()) {
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
      f1       = uint32_t(f0+1)%uint32_t(mover_keyframes.size());
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
    invalidateView();
    disableTicks();

    if(!target.empty()) {
      TriggerEvent e(target,vobName,TriggerEvent::T_Activate);
      world.triggerEvent(e);
      }

    std::string_view snd = sfxOpenEnd;
    if(prev==Close)
      snd = sfxCloseEnd;
    if(prev==NextKey)
      snd = "";
    if(behavior==phoenix::mover_behavior::open_timed && prev==Open) {
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
