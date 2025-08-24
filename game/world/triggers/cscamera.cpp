#include "cscamera.h"

#include <Tempest/Log>
#include <cassert>

#include "gothic.h"

using namespace Tempest;

Vec3 CsCamera::KeyFrame::position(float u) const {
  return ((c[0]*u + c[1])*u + c[2])*u + c[3];
  }

float CsCamera::KeyFrame::arcLength() const {
  float len  = 0;
  Vec3  prev = c[3];
  for(int i=1; i<=100; ++i) {
    Vec3 at = position(float(i)/100.f);
    len += (at-prev).length();
    prev = at;
    }
  return len;
  }

CsCamera::KbSpline::KbSpline(const std::vector<std::shared_ptr<zenkit::VCameraTrajectoryFrame> >& frames, const float duration, std::string_view vobName) {
  keyframe.reserve(frames.size());
  for(auto& f : frames) {
    KeyFrame kF;
    kF.c[3]  = Vec3(f->original_pose[3][0],f->original_pose[3][1],f->original_pose[3][2]);
    kF.time  = f->time;
    keyframe.push_back(kF);
    }

  const size_t size = keyframe.size();
  float pathLength = 0;
  for(size_t i=0; i+1<size; ++i) {
    auto& kF  = keyframe[i];
    Vec3  p0  = i==0 ? kF.c[3] : keyframe[i-1].c[3];
    Vec3  p1  = kF.c[3];
    Vec3  p2  = keyframe[i+1].c[3];
    Vec3  p3  = i+2==size ? kF.c[3] : keyframe[i+2].c[3];
    Vec3  dd  = (p2-p0)*0.5f;
    Vec3  sd  = (p3-p1)*0.5f;
    kF.c[0] =  2 * p1 - 2*p2 +   dd + sd;
    kF.c[1] = -3 * p1 + 3*p2 - 2*dd - sd;
    kF.c[2] = std::move(dd);

    pathLength += kF.arcLength();
    }

  if(size<2)
    return;

  if(keyframe.front().time!=0) {
    Log::e("CsCamera: \"", vobName, "\" - invalid first frame");
    }
  if(keyframe.back().time!=duration) {
    Log::e("CsCamera: \"", vobName, "\" - invalid sequence duration");
    }

  const float ref    = 1.f/std::max(pathLength, 0.0001f);
  const float slow   = 0;
  const float linear = duration * ref;
  const float fast   = 2 * duration * ref;

  const zenkit::CameraMotion mType0 = frames.front()->motion_type;
  const zenkit::CameraMotion mType1 = frames.back() ->motion_type;

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

  this->c[0] = -2*duration +   d0 + d1;
  this->c[1] =  3*duration - 2*d0 - d1;
  this->c[2] = d0;
  }

float CsCamera::KbSpline::applyMotionScaling(float t) const {
  return std::min(((c[0]*t + c[1])*t + c[2])*t, keyframe.back().time);
  }

Vec3 CsCamera::KbSpline::position(float time) const {
  const float t = applyMotionScaling(time);

  //TODO: lower bound
  uint32_t n = 0;
  while(n+1<keyframe.size() && t>=keyframe[n+1].time) {
    ++n;
    }

  auto& kF0 = keyframe[n];
  auto& kF1 = keyframe[n+1];
  float u   = (t - kF0.time) / (kF1.time - kF0.time);
  assert(0<=u && u<=1);

  return keyframe[n].position(u);
  }


CsCamera::CsCamera(Vob* parent, World& world, const zenkit::VCutsceneCamera& cam, Flags flags)
  :AbstractTrigger(parent,world,cam,flags) {
  if(cam.position_count<1 || cam.total_duration<=0)
    return;

  if(cam.trajectory_for==zenkit::CameraCoordinateReference::OBJECT || cam.target_trajectory_for==zenkit::CameraCoordinateReference::OBJECT) {
    Log::d("Object camera not implemented, \"", name() , "\"");
    return;
    }

  duration      = uint64_t(cam.total_duration * 1000.f);
  delay         = uint64_t(cam.auto_untrigger_last_delay * 1000.f);
  autoUntrigger = cam.auto_untrigger_last;
  playerMovable = cam.auto_player_movable;

  posSpline    = KbSpline(cam.trajectory_frames, cam.total_duration, cam.vob_name);
  targetSpline = KbSpline(cam.target_frames, cam.total_duration, cam.vob_name);
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
    pos = posSpline.position(float(time)/float(duration));
    }
  return pos;
  }

PointF CsCamera::spin(Tempest::Vec3& d) {
  if(targetSpline.size()==0)
    d = d - Gothic::inst().camera()->destPosition();
  else if(targetSpline.size()==1)
    d = targetSpline.keyframe[0].c[3] - d;
  else if(targetSpline.size()>1) {
    d = targetSpline.position(float(time)/float(duration)) - d;
    }

  float k     = 180.f/float(M_PI);
  float spinX = k * std::asin(d.y/d.length());
  float spinY = -90;
  if(d.x!=0.f || d.z!=0.f)
    spinY = 90 + k * std::atan2(d.z,d.x);
  return {-spinX,spinY};
  }
