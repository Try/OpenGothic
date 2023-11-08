#include "cscamera.h"

#include <Tempest/Log>

#include "gothic.h"

using namespace Tempest;

CsCamera::CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& cam, Flags flags)
  :AbstractTrigger(parent,world,cam,flags) {

  if(cam.position_count<1 || cam.total_duration<0)
    return;

  if(cam.trajectory_for==phoenix::camera_trajectory::object || cam.target_trajectory_for==phoenix::camera_trajectory::object) {
    Log::d("Object camera not implemented, \"", name() , "\"");
    return;
    }

  duration = cam.total_duration;
  delay    = cam.auto_untrigger_last_delay;

  for(uint32_t i=0;i<cam.frames.size();++i) {
    auto&    f   = cam.frames[i];
    KeyFrame kF;
    kF.c[3]  = Vec3(f->original_pose[3][0],f->original_pose[3][1],f->original_pose[3][2]);
    kF.time  = f->time;
    if(i<uint32_t(cam.position_count))
      posSpline.keyframe.push_back(std::move(kF)); else
      targetSpline.keyframe.push_back(std::move(kF));
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
    assert(spl->keyframe.front().time==0);
    assert(spl->keyframe.back().time==duration);
    const float slow   = 0;
    const float linear = duration;
    const float fast   = 2 * duration;
    uint32_t    start  = spl==&posSpline ? 0 : uint32_t(cam.position_count);
    uint32_t    end    = spl==&posSpline ? uint32_t(cam.position_count-1) : uint32_t(cam.frames.size()-1);
    auto        mType0 = cam.frames[start]->motion_type;
    auto        mType1 = cam.frames[end]->motion_type;
    float       d0     = slow;
    float       d1     = slow;
    if(mType0!=phoenix::camera_motion::slow && mType1!=phoenix::camera_motion::slow) {
      d0 = linear;
      d1 = linear;
      }
    else if(mType0==phoenix::camera_motion::slow && mType1!=phoenix::camera_motion::slow) {
      d0 = slow;
      d1 = fast;
      }
    else if(mType0!=phoenix::camera_motion::slow && mType1==phoenix::camera_motion::slow) {
      d0 = fast;
      d1 = slow;
      }

    spl->c[0] = -2*duration +   d0 + d1;
    spl->c[1] =  3*duration - 2*d0 - d1;
    spl->c[2] = d0;
    }
  }

void CsCamera::onTrigger(const TriggerEvent& evt) {
  if(active || posSpline.size()==0)
    return;

  auto world = Gothic::inst().world();
  active               = true;
  time                 = 0;
  posSpline.splTime    = 0;
  targetSpline.splTime = 0;
  if(world->currentCs()==nullptr) {
    auto camera = Gothic::inst().camera();
    camera->reset();
    camera->setMode(Camera::Mode::Normal);
    godMode = Gothic::inst().isGodMode();
    Gothic::inst().setGodMode(true);
    }
  world->setCurrentCs(this);
  enableTicks();
  }

void CsCamera::onUntrigger(const TriggerEvent& evt) {
  clear();
  }

void CsCamera::clear() {
  if(!active)
    return;
  active = false;
  disableTicks();
  auto world = Gothic::inst().world();
  if(world->currentCs()==this) {
    auto camera = Gothic::inst().camera();
    world->setCurrentCs(nullptr);
    camera->reset();
    Gothic::inst().setGodMode(godMode);
    }
  }

void CsCamera::tick(uint64_t dt) {
  auto world = Gothic::inst().world();
  time += float(dt)/1000.f;

  if(time>duration+delay || world->currentCs()!=this) {
    clear();
    return;
    }

  if(time>duration)
    return;

  auto camera = Gothic::inst().camera();
  auto cPos   = position();
  camera->setPosition(cPos);
  camera->setSpin(spin(cPos));
  }

Vec3 CsCamera::position() {
  Vec3 pos;
  if(posSpline.size()==1) {
    pos = posSpline.keyframe[0].c[3];
    } else {
    posSpline.setSplTime(time/duration);
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
    targetSpline.setSplTime(time/duration);
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
