#include "cscamera.h"

#include <Tempest/Log>
#include <cassert>
#include "utils/dbgpainter.h"
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


CsCamera::KbSpline::KbSpline(const std::vector<std::shared_ptr<zenkit::VCameraTrajectoryFrame>>& frames, const float duration, std::string_view vobName) {
  keyframe.reserve(frames.size());
  for(auto& f : frames) {
    KeyFrame kF;
    kF.c[3]  = Vec3(f->original_pose[3][0],f->original_pose[3][1],f->original_pose[3][2]);
    kF.time  = f->time;
    kF.motionType = f->motion_type;
    keyframe.push_back(kF);
    }

  const size_t size = keyframe.size();
  for(size_t i=0; i+1<size; ++i) {
    auto& kF0 = i==0 ? keyframe[i+0] : keyframe[i-1];
    auto& kF1 = keyframe[i+0];
    auto& kF2 = keyframe[i+1];
    auto& kF3 = i+2==size ? kF2 : keyframe[i+2];

    auto& p0  = kF0.c[3];
    auto& p1  = kF1.c[3];
    auto& p2  = kF2.c[3];
    auto& p3  = kF3.c[3];

    float dt  = kF2.time-kF1.time;
    Vec3  dd  = (p2-p0) * dt/(kF2.time-kF0.time);
    Vec3  sd  = (p3-p1) * dt/(kF3.time-kF1.time);

    auto& kF  = keyframe[i];
    kF.c[0] =  2 * p1 - 2*p2 +   dd + sd;
    kF.c[1] = -3 * p1 + 3*p2 - 2*dd - sd;
    kF.c[2] = std::move(dd);
    }

  if(size<2)
    return;

  if(keyframe.front().time!=0) {
    Log::e("CsCamera: \"", vobName, "\" - invalid first frame");
    }
  if(keyframe.back().time!=duration) {
    Log::e("CsCamera: \"", vobName, "\" - invalid sequence duration");
    }
  }

Vec3 CsCamera::KbSpline::position(const uint64_t time) const {
  const float t = float(time)/1000.f;

  //TODO: lower bound
  uint32_t n = 0;
  while(n+1<keyframe.size() && t>=keyframe[n+1].time) {
    ++n;
    }

  auto& kF0 = keyframe[n];
  auto& kF1 = keyframe[n+1];
  float u   = (t - kF0.time) / (kF1.time - kF0.time);

  const float k = 1.005f;
  if(kF0.motionType==zenkit::CameraMotion::SLOW && kF1.motionType!=zenkit::CameraMotion::SLOW) {
    // slow -> non-slow = accelerate
    u = std::pow(u, k);
    }
  else if(kF0.motionType!=zenkit::CameraMotion::SLOW && kF1.motionType==zenkit::CameraMotion::SLOW) {
    // non-slow -> slow = deccelerate
    u = std::pow(u, 1.f/k);
    }

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

void CsCamera::debugDraw(DbgPainter& p) const {
  static bool dbg = false;
  if(!dbg)
    return;

  if(posSpline.size()<=1)
    return;

  auto at = posSpline.keyframe[0].position(0);
  for(size_t i=1; i<posSpline.size(); ++i) {
    auto& k   = posSpline.keyframe[i];
    p.setPen(Color(0,0,1));
    for(size_t r=0; r<100; ++r) {
      auto  pos = k.position(float(r)/100.f);
      p.drawLine(at, pos);
      at = pos;
      }

    auto  pt = k.position(0);
    float l  = 30;
    p.setPen(Color(1,0,1));
    p.drawLine(pt-Vec3(l,0,0),pt+Vec3(l,0,0));
    p.drawLine(pt-Vec3(0,l,0),pt+Vec3(0,l,0));
    p.drawLine(pt-Vec3(0,0,l),pt+Vec3(0,0,l));
    }
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

  active      = true;
  timeTrigger = world.tickCount();
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

  auto& camera = world.gameSession().camera();
  camera.setMode(Camera::Mode::Normal);
  camera.reset();
  }

void CsCamera::tick(uint64_t /*dt*/) {
  const uint64_t time = world.tickCount() - timeTrigger;
  if(time>duration+delay && (autoUntrigger || vobName=="TIMEDEMO")) {
    TriggerEvent e("","",TriggerEvent::T_Untrigger);
    onUntrigger(e);
    return;
    }

  if(time>duration)
    return;

  auto& camera = world.gameSession().camera();
  if(camera.isCutscene()) {
    auto cPos = position();
    camera.setPosition(cPos);
    camera.setSpin(spin(cPos));
    }
  }

Vec3 CsCamera::position() {
  Vec3 pos;
  if(posSpline.size()==1) {
    pos = posSpline.keyframe[0].c[3];
    } else {
    const uint64_t time = std::min(world.tickCount() - timeTrigger, duration);
    pos = posSpline.position(time);
    }
  return pos;
  }

PointF CsCamera::spin(Tempest::Vec3& d) {
  if(targetSpline.size()==0)
    d = d - Gothic::inst().camera()->destPosition();
  else if(targetSpline.size()==1)
    d = targetSpline.keyframe[0].c[3] - d;
  else if(targetSpline.size()>1) {
    const uint64_t time = std::min(world.tickCount() - timeTrigger, duration);
    d = targetSpline.position(time) - d;
    }

  float k     = 180.f/float(M_PI);
  float spinX = k * std::asin(d.y/d.length());
  float spinY = -90;
  if(d.x!=0.f || d.z!=0.f)
    spinY = 90 + k * std::atan2(d.z,d.x);
  return {-spinX,spinY};
  }
