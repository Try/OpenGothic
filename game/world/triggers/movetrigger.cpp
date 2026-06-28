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
  stayOpenTime    = uint64_t(mover.stay_open_time_sec * 1000.f);
  sfxOpenEnd      = mover.sfx_open_end;
  sfxCloseStart   = mover.sfx_close_start;
  sfxCloseEnd     = mover.sfx_close_end;
  sfxMoving       = mover.sfx_transitioning;
  visualName      = mover.visual->name;

  if(mover.cd_dynamic || mover.cd_static) {
    if(auto mesh = Resources::loadMesh(mover.visual->name))
      physic = PhysicMesh(*mesh,*world.physic(),true);
    }

  const float speed = mover.speed;
  keyframes.resize(moverKeyFrames.size());
  for(size_t i=0; i<moverKeyFrames.size(); ++i) {
    auto& f0 = moverKeyFrames[i];
    auto& f1 = moverKeyFrames[(i+1)%moverKeyFrames.size()];
    auto  dx = (f1.position.x-f0.position.x);
    auto  dy = (f1.position.y-f0.position.y);
    auto  dz = (f1.position.z-f0.position.z);

    float theta = 2.f*std::pow(f1.rotation.x*f0.rotation.x + f1.rotation.y*f0.rotation.y + f1.rotation.z*f0.rotation.z + f1.rotation.w*f0.rotation.w, 2.f) - 1.f;
    float angle = float(std::acos(std::clamp(theta, -1.f, 1.f))*180.0/M_PI);
    float len   = Vec3(dx,dy,dz).length();

    if(speed>0) {
      /*
       * Distance between keyframe positions doesn't have to be exactly zero to use rotation speed,
       * it seem there is some small threshold.
       * Otherwise the boat close to Lares in L'Hiver mod would jump between two keyframes.
       */
      static const float rotationThreshold = 0.01f;

      uint64_t ticks = 0;
      if(len>rotationThreshold)
        ticks = uint64_t(len/speed); else
        ticks = uint64_t((angle/360.f) * 1000.f/scaleRotSpeed(speed));
      keyframes[i].ticks = std::max<uint64_t>(1,ticks);
      }
    }

  auto tr = transform();
  if(frame<moverKeyFrames.size())
    tr = mkMatrix(moverKeyFrames[frame]);
  tr.inverse();
  pos0 = localTransform();
  pos0.mul(tr);

  invalidateView();
  MoveTrigger::moveEvent();
  }

void MoveTrigger::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(pos0,uint8_t(state),frameTime,frame,targetFrame);
  // NOTE: in original-game zCMover::Archive (Gothic2.exe @0x006137e0) the TRIGGER_CONTROL ref count
  // (field_0x198, incremented in TriggerMover @0x00612cb0, decremented in OnUntrigger @0x00613170)
  // is persisted in the savegame block; restore it so a mover opened by several sources still
  // requires all of them to untrigger before closing.
  fout.write(triggerCount);
  }

void MoveTrigger::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),frameTime,frame);
  if(fin.version()>49)
    fin.read(targetFrame);
  if(fin.version()>56)
    fin.read(triggerCount);
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
  if(moverKeyFrames.size()<2 || keyframes[0].ticks==0)
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
      // NOTE: in original-game zCMover::TriggerMover (Gothic2.exe 0x00612cb0) each OnTrigger
      // increments a ref count; the mover closes (OnUntrigger) only once all sources have
      // untriggered. Persisted across save/load (v57) to match zCMover::Archive @0x006137e0.
      ++triggerCount;
      if(frame==0 && state==Idle)
        state = Open;
      break;
      }
    case zenkit::MoverBehavior::OPEN_TIME: {
      // NOTE: in original-game zCMover::TriggerMover (Gothic2.exe @0x00612cb0) a re-trigger of a
      // fully-open OPEN_TIME mover collapses the stay-open countdown (field_0x19c) to ~1ms so the
      // mover closes on the next tick (OnTick @0x00612f80), instead of restarting the full duration.
      // Setting frameTime=stayOpenTime makes the next tick()'s OpenTimed branch transition to Close
      // immediately, reproducing "re-trigger closes" (OG kept it open, indefinitely under a repeater).
      if(state==OpenTimed)
        frameTime = stayOpenTime; else
        state     = Open;
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
  if(keyframes.size()<2 || keyframes[0].ticks==0)
    return;
  if(behavior!=zenkit::MoverBehavior::TRIGGER_CONTROL)
    return;
  // NOTE: in original-game zCMover::OnUntrigger (Gothic2.exe 0x00613170) the close begins
  // only when the trigger ref count drops below 1; a single untrigger must not close a mover
  // that several triggers opened.
  if(triggerCount>0)
    --triggerCount;
  if(triggerCount>0)
    return;
  if(frame>0 && state==Idle) {
    state       = Close;
    targetFrame = 0;
    preProcessTrigger();
    }
  }

void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(keyframes.size()<2 || keyframes[0].ticks==0)
    return;
  if(evt.move.key<0 || keyframes.size()<size_t(evt.move.key))
    return;
  if(behavior!=zenkit::MoverBehavior::SINGLE_KEYS)
    return;
  if(state!=Idle)
    return;
  state = SingleKey;
  switch(evt.move.msg) {
    case zenkit::MoverMessageType::NEXT:
      // NOTE: in original-game zCMover::OnMessage (Gothic2.exe 0x00613450) NEXT on a
      // SINGLE_KEYS mover wraps: target = (curFrame+1) % keyframeCount (nextFrame only wraps
      // for LOOP behavior, so it got stuck on the last keyframe).
      targetFrame = (frame + 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::PREVIOUS:
      // NOTE: original PREVIOUS wraps: target = curFrame-1, or keyframeCount-1 if negative.
      targetFrame = (frame + uint32_t(keyframes.size()) - 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::FIXED_DIRECT:
    case zenkit::MoverMessageType::FIXED_ORDER:
      targetFrame = uint32_t(evt.move.key);
      break;
    }
  preProcessTrigger();
  }

void MoveTrigger::preProcessTrigger(State prev) {
  if(prev==state || state==Idle)
    return;
  if(state==Open) {
    targetFrame = uint32_t(keyframes.size())-1;
    emitSound(sfxOpenStart);
    if(prev==Close) {
      // NOTE: in original-game zCMover::InvertMovement @0x00612300 (reached from TriggerMover
      // @0x00612cb0 when a 2STATE mover is re-triggered while moving) reversal negates the per-tick
      // frame velocity at the SAME continuous keyframe position, keeping the mover inside its current
      // segment. During Close `frame` is the segment's upper index [prevFrame(frame),frame]; reversing
      // to Open it must become the lower index prevFrame(frame) of that same segment, with frameTime
      // rebased on that lower index's ticks. OpenGothic moved frame to nextFrame (an adjacent segment)
      // with the wrong ticks base, so frame==targetFrame fired next tick and froze the mover mid-travel.
      frame     = prevFrame(frame);
      frameTime = keyframes[frame].ticks - frameTime;
      return;
      }
    }
  else if(state==Close) {
    targetFrame = 0;
    emitSound(sfxCloseStart);
    if(prev==Open) {
      // NOTE: see zCMover::InvertMovement @0x00612300 -- reversing Open->Close stays in the same
      // segment [frame,nextFrame(frame)]; during Open `frame` is the lower index, so for Close it must
      // become the upper index nextFrame(frame). frameTime is rebased on the lower index's ticks
      // (the old `frame`, read before reassigning). OpenGothic dropped into an adjacent segment.
      frameTime = keyframes[frame].ticks - frameTime;
      frame     = nextFrame(frame);
      return;
      }
    }
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
    return;
    }
  state = Idle;
  invalidateView();
  disableTicks();
  }

uint32_t MoveTrigger::nextFrame(uint32_t i) const {
  uint32_t size = uint32_t(keyframes.size());
  if(behavior==zenkit::MoverBehavior::LOOP)
    return (i+1)%size;
  return std::min<uint32_t>(i+1, size-1);
  }

uint32_t MoveTrigger::prevFrame(uint32_t i) const {
  uint32_t size = uint32_t(keyframes.size());
  if(behavior==zenkit::MoverBehavior::LOOP)
    return (i+size-1)%size;
  return std::max<uint32_t>(i, 1) - 1;
  }

void MoveTrigger::tick(uint64_t dt) {
  assert(state!=Idle);
  frameTime += dt;
  if(state==OpenTimed) {
    if(frameTime<=stayOpenTime)
      return;
    state       = Close;
    targetFrame = 0;
    frameTime  -= stayOpenTime;
    }
  if(frame==targetFrame) {
    postProcessTrigger();
    return;
    }
  advanceAnim();
  updateFrame();
  emitSound(sfxMoving);
  }

void MoveTrigger::updateFrame() {
  uint32_t f = (state==Close) ? prevFrame(frame) : frame;
  if(frameTime<keyframes[f].ticks)
    return;
  if(state==Close)
    frame = prevFrame(frame); else
    frame = nextFrame(frame);
  frameTime = 0;
  }

 void MoveTrigger::advanceAnim() {
  uint32_t f1  = (state==Close) ? prevFrame(frame) : frame;
  uint32_t f2  = nextFrame(f1);
  float    a   = calcProgress(f1,f2);
  auto     m   = calcTransform(f1,f2,a);
  auto     mat = pos0;
  mat.mul(m);
  setLocalTransform(mat);
  }

float MoveTrigger::calcProgress(uint32_t f1, uint32_t f2) const {
  float a = float(frameTime)/float(keyframes[f1].ticks);
  a = std::clamp(a,0.f,1.f);
  if(state==Close)
    a = 1 - a;
  bool start = (f1==0 && state!=Loop);
  bool end   = (f2==moverKeyFrames.size()-1 && state!=Loop);
  // NOTE: in original-game zCMover::InterpolateKeyframes_KF (Gothic2.exe 0x00611900) the
  // intra-segment easing is sinusoidal, not polynomial: smoothstep = 0.5*(1-cos(a*PI)),
  // slow-start = 1-cos(a*PI/2), slow-end = sin(a*PI/2). zSinApprox/zCosApprox are LUT sin/cos, so
  // std::sin/std::cos reproduce the curves. The old polynomials matched at the endpoints but
  // diverged mid-segment (slow-start/slow-end by up to ~9%), giving wrong partway platform/door
  // positions (relevant to the move-trigger glide complaints, issues #637/#623).
  const float smoothStep = 0.5f*(1.f - std::cos(a*float(M_PI)));
  const float slowStart  = 1.f - std::cos(a*float(M_PI)*0.5f);
  const float slowEnd    = std::sin(a*float(M_PI)*0.5f);
  switch(speedType) {
    case zenkit::MoverSpeedType::CONSTANT:
      break;
    case zenkit::MoverSpeedType::SLOW_START_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START_END:
      if((start && end) || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START_END)
        a = smoothStep;
      else if(start)
        a = slowStart;
      else if(end)
        a = slowEnd;
      break;
    case zenkit::MoverSpeedType::SLOW_START:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START:
      if(start || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START)
        a = slowStart;
      break;
    case zenkit::MoverSpeedType::SLOW_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_END:
      if(end || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_END)
        a = slowEnd;
      break;
    }
  return std::clamp(a,0.f,1.f);
  }

Matrix4x4 MoveTrigger::calcTransform(uint32_t f1, uint32_t f2, float a) const {
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
