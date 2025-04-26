#include "cscamera.h"

#include <Tempest/Log>
#include <cassert>

#include "gothic.h"

using namespace Tempest;

CsCamera::CsCamera(Vob* parent, World& world, const zenkit::VCutsceneCamera& cam, Flags flags)
  :AbstractTrigger(parent,world,cam,flags) {
  if(cam.position_count<1 || cam.total_duration<0)
    return;

  if(cam.trajectory_for==zenkit::CameraCoordinateReference::OBJECT || cam.target_trajectory_for==zenkit::CameraCoordinateReference::OBJECT) {
    Log::d("Object camera not implemented, \"", name() , "\"");
    return;
    }

  durationF     = cam.total_duration;
  duration      = uint64_t(cam.total_duration * 1000.f);
  delay         = uint64_t(cam.auto_untrigger_last_delay * 1000.f);
  autoUntrigger = cam.auto_untrigger_last;
  playerMovable = cam.auto_player_movable;

  for(auto& f : cam.trajectory_frames) {
    KeyFrame kF;
    kF.c[3]  = Vec3(f->original_pose[3][0],f->original_pose[3][1],f->original_pose[3][2]);
    kF.time  = f->time;
    posSpline.keyframe.push_back(kF);
    }

  for(auto& f : cam.target_frames) {
    KeyFrame kF;
    kF.c[3]  = Vec3(f->original_pose[3][0],f->original_pose[3][1],f->original_pose[3][2]);
    kF.time  = f->time;
    targetSpline.keyframe.push_back(kF);
    }

  for(auto spl : {&posSpline,&targetSpline}) {
    uint32_t size = uint32_t(spl->size());
    for(uint32_t i=0;i+1<size;++i) {
      auto& kF  = spl->keyframe[i];
      Vec3  p0  = i==0 ? kF.c[3] : spl->keyframe[i-1].c[3];
      Vec3  p1  = kF.c[3];
      Vec3  p2  = spl->keyframe[i+1].c[3];
      Vec3  p3  = i+2==size ? kF.c[3] : spl->keyframe[i+2].c[3];
      Vec3  dd  = (p2-p0)*0.5f;
      Vec3  sd  = (p3-p1)*0.5f;
      kF.c[0] =  2 * p1 - 2*p2 +   dd + sd;
      kF.c[1] = -3 * p1 + 3*p2 - 2*dd - sd;
      kF.c[2] = std::move(dd);
      }

    if(size<2)
      continue;

    if(spl->keyframe.front().time!=0) {
      Log::e("CsCamera: \"",cam.vob_name,"\" - invalid first frame");
      }
    if(spl->keyframe.back().time!=durationF) {
      Log::e("CsCamera: \"",cam.vob_name,"\" - invalid sequence duration");
      }

    const float slow   = 0;
    const float linear = durationF;
    const float fast   = 2 * durationF;

    zenkit::CameraMotion mType0, mType1;
    if(spl == &posSpline) {
      mType0 = cam.trajectory_frames[0]->motion_type;
      mType1 = cam.trajectory_frames.back()->motion_type;
      } else {
      mType0 = cam.target_frames[0]->motion_type;
      mType1 = cam.target_frames.back()->motion_type;
      }

    float d0 = slow;
    float d1 = slow;
    if(mType0!=zenkit::CameraMotion::SLOW && mType1!=zenkit::CameraMotion::SLOW) {
      d0 = linear;
      d1 = linear;
      }
    else if(mType0==zenkit::CameraMotion::SLOW && mType1!=zenkit::CameraMotion::SLOW) {
      d0 = slow;
      d1 = fast;
      }
    else if(mType0!=zenkit::CameraMotion::SLOW && mType1==zenkit::CameraMotion::SLOW) {
      d0 = fast;
      d1 = slow;
      }

    spl->c[0] = -2*durationF +   d0 + d1;
    spl->c[1] =  3*durationF - 2*d0 - d1;
    spl->c[2] = d0;
    }
  }

bool CsCamera::isPlayerMovable() const {
  return playerMovable;
  }

void CsCamera::onTrigger(const TriggerEvent& evt) {
  if(active || posSpline.size()==0)
    return;

  if(auto cs = world.currentCs())
    cs->onUntrigger(evt);

  auto& camera = world.gameSession().camera();
  if(!camera.isCutscene()) {
    camera.reset();
    camera.setMode(Camera::Mode::Cutscene);
    }

  active               = true;
  time                 = 0;
  posSpline.splTime    = 0;
  targetSpline.splTime = 0;
  godMode              = Gothic::inst().isGodMode();
  Gothic::inst().setGodMode(true);
  world.setCurrentCs(this);
  enableTicks();
  }

void CsCamera::onUntrigger(const TriggerEvent& evt) {
  if(!active)
    return;
  active = false;
  disableTicks();
  if(world.currentCs()!=this)
    return;

  world.setCurrentCs(nullptr);
  Gothic::inst().setGodMode(godMode);

  auto& camera = world.gameSession().camera();
  camera.setMode(Camera::Mode::Normal);
  camera.reset();
  }

void CsCamera::tick(uint64_t dt) {
  time += dt;

  if(time>duration+delay && (autoUntrigger || vobName=="TIMEDEMO")) {
    TriggerEvent e("","",TriggerEvent::T_Untrigger);
    onUntrigger(e);
    return;
    }

  if(time>duration)
    return;

  auto& camera = world.gameSession().camera();
  if(camera.isCutscene()) {
    auto cPos   = position();
    camera.setPosition(cPos);
    camera.setSpin(spin(cPos));
    }
  }

Vec3 CsCamera::position() {
  Vec3 pos;
  if(posSpline.size()==1) {
    pos = posSpline.keyframe[0].c[3];
    } else {
    posSpline.setSplTime(float(time)/float(duration));
    pos = posSpline.position();
    }
  return pos;
  }

PointF CsCamera::spin(Tempest::Vec3& d) {
  if(targetSpline.size()==0)
    d = d - Gothic::inst().camera()->destPosition();
  else if(targetSpline.size()==1)
    d = targetSpline.keyframe[0].c[3] - d;
  else if(targetSpline.size()>1) {
    targetSpline.setSplTime(float(time)/float(duration));
    d = targetSpline.position() - d;
    }

  float k     = 180.f/float(M_PI);
  float spinX = k * std::asin(d.y/d.length());
  float spinY = -90;
  if(d.x!=0.f || d.z!=0.f)
    spinY = 90 + k * std::atan2(d.z,d.x);
  return {-spinX,spinY};
  }

void CsCamera::KbSpline::setSplTime(float time) {
  float    t   = applyMotionScaling(time);
  uint32_t n   = uint32_t(splTime);
  auto     kF0 = &keyframe[n];
  auto     kF1 = &keyframe[n+1];
  if(t>kF1->time) {
    splTime = std::ceil(splTime);
    n       = uint32_t(splTime);
    kF0     = kF1;
    kF1     = &keyframe[n+1];
    }
  assert(n<size());
  float u = (t - kF0->time) / (kF1->time - kF0->time);
  assert(u>=0 && u<=1);
  splTime = float(n) + u;
  }

Vec3 CsCamera::KbSpline::position() const {
  float n;
  float t  = std::modf(splTime,&n);
  auto& kF = keyframe[uint32_t(n)];
  return ((kF.c[0]*t + kF.c[1])*t + kF.c[2])*t + kF.c[3];
  }

float CsCamera::KbSpline::applyMotionScaling(float t) const {
  return std::min(((c[0]*t + c[1])*t + c[2])*t,keyframe.back().time);
  }
