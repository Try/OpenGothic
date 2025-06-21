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
  visualName      = mover.visual->name;

  if(mover.cd_dynamic || mover.cd_static) {
    if(auto mesh = Resources::loadMesh(mover.visual->name))
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

    float theta = 2.f*std::pow(f1.rotation.x*f0.rotation.x + f1.rotation.y*f0.rotation.y + f1.rotation.z*f0.rotation.z + f1.rotation.w*f0.rotation.w, 2.f) - 1.f;
    float angle = float(std::acos(std::clamp(theta, -1.f, 1.f))*180.0/M_PI);
    float len   = Vec3(dx,dy,dz).length();

    if(speed>0) {
      uint64_t ticks = 0;
      if(len>0)
        ticks = uint64_t(len/speed); else
        ticks = uint64_t((angle/360.f) * 1000.f/scaleRotSpeed(speed));
      keyframes[i].ticks = std::max<uint64_t>(1,ticks);
      }
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

float MoveTrigger::scaleRotSpeed(float speed) const {
  // Measurements for rotation only movers show contradictory values without a clear path to translate speed attribute into actual movement speed.
  // Assume speed is given as rotations/s and scale movers which would move extremely slow compared to vanilla.
  // first column : time measured needed for one rotation
  // second column: speed attribute
  // third column : tells us how much faster this mover is compared to a mover that would take speed as rot/s.

  //                                 time    speed    1/(time*speed)
  // Newworld
  // NW_MISC_WINDMILL_ROTOR_01        44.40  0.02       1.13
  // NW_HARBOUR_BOAT_03              271.04  0.0005     7.38
  // EVT_TROLL_GRAVE_MOVER_01         19.20  0.0001   520.83
  // EVT_CITY_REICH03_01               4.48  0.005     44.67

  // Irdorath
  // EVT_RINGMAIN_LEFT_01              3.40  0.001    294.12
  // EVT_RIGHT_WHEEL_01                5.00  0.00068  294.12
  // EVT_RIGHT_WHEEL_02                5.00  0.2        1.00
  // EVT_RIGHT_ROOM_01_SPAWN_ROT_02    2.8   0.001    357.14
  // EVT_LEFT_ROOM_02_SPAWN_ROT_02     2.56  0.0005   781.25

  // Addonworld Adanos temple door
  // EVT_ADDON_LSTTEMP_DOOR_LEFT_01   16.66  0.00028  214.42
  // EVT_ADDON_LSTTEMP_DOOR_RIGHT_01  16.97  0.2        0.29

  // Use a general scaling factor and specialize for boats only where low speed is intended.
  const float sFactor     = 300;
  const float sFactorBoat = 7;
  const float lowSpeed    = 0.006f;
  if(world.name()=="newworld.zen" && vobName.starts_with("NW_HARBOUR_BOAT_"))
    return sFactorBoat*speed;
  if(speed<lowSpeed)
    return sFactor*speed;
  return speed;
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
  if(state==Loop)
    return;
  if(state!=Idle) {
    // FIXME: can lead to infinite amount of events if multiple movers with same name exist
    world.triggerEvent(e);
    return;
    }

  std::string_view snd = sfxOpenStart;
  switch(behavior) {
    case zenkit::MoverBehavior::TOGGLE: {
      if(frame==0) {
        state = Open;
        } else {
        state = Close;
        }
      break;
      }
    case zenkit::MoverBehavior::TRIGGER_CONTROL: {
      if(onTrigger)
        state = Open; else
        state = Close;
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
    case Open: {
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
    case Close: {
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

    std::string_view snd = sfxCloseEnd;
    if(prev==Open)
      snd = sfxOpenEnd;
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
