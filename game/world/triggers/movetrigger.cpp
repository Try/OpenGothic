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
  speedType       = mover.speed_mode;
  lerpType        = mover.lerp_mode;
  autoRotate      = mover.auto_rotate;
  sfxOpenStart    = mover.sfx_open_start;
  stayOpenTimeSec = uint64_t(mover.stay_open_time_sec * 1000.f);
  sfxOpenEnd      = mover.sfx_open_end;
  sfxCloseStart   = mover.sfx_close_start;
  sfxCloseEnd     = mover.sfx_close_end;
  sfxMoving       = mover.sfx_transitioning;
  visualName      = mover.visual->name;

  if(mover.cd_dynamic || mover.cd_static) {
    if(auto mesh = Resources::loadMesh(mover.visual->name))
      physic = PhysicMesh(*mesh,*world.physic(),true);
    }

  // rotation only movers don't provide a clear way to translate speed attribute into actual movement speed
  // assume speed is given as rotations/s and scale movers which would move extremely slow compared to vanilla
  float       speed  = mover.speed;
  const float factor = 300.f;
  if(vobName=="EVT_ADDON_LSTTEMP_DOOR_LEFT_01" || vobName=="EVT_RIGHT_WHEEL_01" || vobName=="EVT_LEFT_WHEEL_02" ||
    vobName=="EVT_RINGMAIN_LEFT_01" || vobName=="EVT_RINGMAIN_RIGHT_01" || vobName=="EVT_TROLL_GRAVE_MOVER_01")
    speed *= factor;
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

    const float min = 0.001f;
    if(speed>0) {
      if(len>min)
        keyframes[i].ticks = uint64_t(len/speed);
      else if(angle>min)
        keyframes[i].ticks = uint64_t((angle/360.f) * 1000.f/speed);
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
  fout.write(pos0,uint8_t(state),time,frame);
  }

void MoveTrigger::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),time,frame);
  if(state!=Idle) {
    invalidateView();
    enableTicks();
    }
  }

void MoveTrigger::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::emitSound(std::string_view snd,bool freeSlot) const {
  auto p   = position();
  auto sfx = ::Sound(world,::Sound::T_Regular,snd,p,0,freeSlot);
  sfx.play();
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
  if(moverKeyFrames.size()<2)
    return;

  auto prev = state;
  switch(behavior) {
    case zenkit::MoverBehavior::TOGGLE: {
      if((frame==0 && state==Idle) || state==Close)
        state = Open; else
        state = Close;
      break;
      }
    case zenkit::MoverBehavior::TRIGGER_CONTROL: {
      if(frame==0 && state==Idle)
        state = Open;
      break;
      }
    case zenkit::MoverBehavior::OPEN_TIME: {
      if(state==OpenTimed)
        time  = 0; else
        state = Open;
      break;
      }
    case zenkit::MoverBehavior::LOOP: {
      state = Loop;
      break;
      }
    case zenkit::MoverBehavior::SINGLE_KEYS: {
      return;
      }
    }
  preProcessTrigger(prev);
  }

void MoveTrigger::onUntrigger(const TriggerEvent& e) {
  if(keyframes.size()<2)
    return;
  if(behavior!=zenkit::MoverBehavior::TRIGGER_CONTROL)
    return;
  if(frame>0 && state==Idle) {
    state = Close;
    preProcessTrigger();
    }
  }

void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(keyframes.size()<2)
    return;
  if(evt.move.key<0 || keyframes.size()<size_t(evt.move.key))
    return;
  if(behavior!=zenkit::MoverBehavior::SINGLE_KEYS)
    return;
  if(state!=Idle)
    return;
  if(evt.move.msg==zenkit::MoverMessageType::NEXT) {
    state = NextKey;
    preProcessTrigger();
    } else {
    Log::e("TODO: MoveTrigger::onGotoMsg");
    }
  }

void MoveTrigger::preProcessTrigger(State prev) {
  if(prev==state || state==Idle)
    return;
  if(state==Open) {
    emitSound(sfxOpenStart);
    if(prev==Close) {
      time  = keyframes[frame].ticks - time;
      frame = nextFrame(frame);
      return;
      }
    }
  else if(state==Close) {
    emitSound(sfxCloseStart);
    if(prev==Open) {
      frame = prevFrame(frame);
      time  = keyframes[frame].ticks - time;
      return;
      }
    }
  time = 0;
  invalidateView();
  enableTicks();
  }

void MoveTrigger::postProcessTrigger() {
  if(!target.empty()) {
    TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
    world.triggerEvent(e);
    }
  if(state==Open)
    emitSound(sfxOpenEnd);
  if(state==Close)
    emitSound(sfxCloseEnd);
  if(behavior==zenkit::MoverBehavior::OPEN_TIME && state==Open) {
    state = OpenTimed;
    time  = 0;
    return;
    }
  state = Idle;
  invalidateView();
  disableTicks();
  }

uint32_t MoveTrigger::nextFrame(uint32_t i) const {
  uint32_t size = uint32_t(keyframes.size());
  return (i+1)%size;
  }

uint32_t MoveTrigger::prevFrame(uint32_t i) const {
  uint32_t size = uint32_t(keyframes.size());
  return (i+size-1)%size;
  }

float MoveTrigger::advanceKey() {
  float a = 0;
  if(state==Close) {
    float ticks = float(keyframes[prevFrame(frame)].ticks);
    a = ticks>0 ? 1 - float(time)/ticks : 0.f;
    if(a<=0) {
      frame = prevFrame(frame);
      a = 1;
      time = 0;
      }
    } else {
    float ticks = float(keyframes[frame].ticks);
    a = ticks>0 ? float(time)/ticks : 1.f;
    if(a>=1) {
      frame = nextFrame(frame);
      a = 0;
      time = 0;
      }
    }
  return a;
  }

void MoveTrigger::tick(uint64_t dt) {
  bool  finished = false;
  float a        = 0;
  time += dt;
  switch(state) {
    case Idle:
      return;
    case OpenTimed: {
      if(time>=stayOpenTimeSec) {
        state = Close;
        time  = 0;
        }
      return;
      }
   case Open: {
      a        = advanceKey();
      finished = (frame==moverKeyFrames.size()-1);
      break;
      }
    case Close: {
      a        = advanceKey();
      finished = (frame==0);
      break;
      }
    case Loop: {
      a = advanceKey();
      break;
      }
    case NextKey: {
      uint32_t f = frame;
      a        = advanceKey();
      finished = (frame==nextFrame(f));
      break;
      }
    }

  advanceAnim(a);
  if(finished)
    postProcessTrigger(); else
    emitSound(sfxMoving);
  }

 void MoveTrigger::advanceAnim(float a) {
  uint32_t f1 = (state==Close) ? prevFrame(frame) : frame;
  uint32_t f2 = nextFrame(f1);
  a = applyMotionScaling(f1,f2,a);
  auto   m   = calculateTransform(f1,f2,a);
  auto   mat = pos0;
  mat.mul(m);
  setLocalTransform(mat);
  }

float MoveTrigger::applyMotionScaling(uint32_t f1, uint32_t f2, float a) const {
  bool start = (f1==0 && state!=Loop);
  bool end   = (f2==moverKeyFrames.size()-1 && state!=Loop);
  switch(speedType) {
    case zenkit::MoverSpeedType::CONSTANT:
      break;
    case zenkit::MoverSpeedType::SLOW_START_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START_END:
      if((start && end) || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START_END)
        a = (3 - 2*a) * a*a;
      else if(start)
        a = (2 - a) * a*a;
      else if(end)
        a = ((1 - a)*a + 1)*a;
      break;
    case zenkit::MoverSpeedType::SLOW_START:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START:
      if(start || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START)
        a = (2 - a) * a*a;
      break;
    case zenkit::MoverSpeedType::SLOW_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_END:
      if(end || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_END)
        a = ((1 - a)*a + 1)*a;
      break;
    }
  return std::clamp(a,0.f,1.f);
  }

Matrix4x4 MoveTrigger::calculateTransform(uint32_t f1, uint32_t f2, float a) const {
  zenkit::AnimationSample fr      = {};
  Vec3                    forward = {};
  if(lerpType==zenkit::MoverLerpType::LINEAR) {
    auto& pos1 = moverKeyFrames[f1].position;
    auto& pos2 = moverKeyFrames[f2].position;
    fr      = mix(moverKeyFrames[f1],moverKeyFrames[f2],a);
    forward = {pos2.x-pos1.x,pos2.y-pos1.y,pos2.z-pos1.z};
    } else {
    auto& kF0 = moverKeyFrames[prevFrame(f1)].position;
    auto& kF1 = moverKeyFrames[f1].position;
    auto& kF2 = moverKeyFrames[f2].position;
    auto& kF3 = moverKeyFrames[nextFrame(f2)].position;
    Vec3  p0  = {kF0.x,kF0.y,kF0.z};
    Vec3  p1  = {kF1.x,kF1.y,kF1.z};
    Vec3  p2  = {kF2.x,kF2.y,kF2.z};
    Vec3  p3  = {kF3.x,kF3.y,kF3.z};
    Vec3  b1  =  -p0 +   p1 - p2 + p3;
    Vec3  b2  = 2*p0 - 2*p1 + p2 - p3;
    Vec3  b3  =  -p0        + p2;
    Vec3  pos = ((b1*a + b2)*a + b3)*a + p1;
    forward       = (3*b1*a + 2*b2)*a + b3;
    fr.position.x = pos.x;
    fr.position.y = pos.y;
    fr.position.z = pos.z;
    fr.rotation   = slerp(moverKeyFrames[f1].rotation,moverKeyFrames[f2].rotation,a);
    }

  if(!autoRotate)
    return mkMatrix(fr);
  Vec3 z  = Vec3::normalize(forward);
  Vec3 up = std::abs(z.y)<0.999f ? Vec3(0,1,0) : Vec3(1,0,0);
  Vec3 x  = Vec3::normalize(Vec3::crossProduct(up,z));
  Vec3 y  = Vec3::crossProduct(z,x);
  return {
         x.x, y.x, z.x, fr.position.x,
         x.y, y.y, z.y, fr.position.y,
         x.z, y.z, z.z, fr.position.z,
         0,   0,   0,   1
         };
  }
