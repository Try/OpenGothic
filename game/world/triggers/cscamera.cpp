#include "cscamera.h"

#include "gothic.h"

using namespace Tempest;

CsCamera::CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& cam, Flags flags)
  :AbstractTrigger(parent,world,cam,flags) {

  if(cam.position_count<1 || cam.total_duration<0)
    return;

  if(cam.trajectory_for==phoenix::camera_trajectory::object || cam.target_trajectory_for==phoenix::camera_trajectory::object)
    return;

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

  init(cam);
  }

void CsCamera::onTrigger(const TriggerEvent& evt) {
  if(active || posSpline.size()==0)
    return;
  active = true;
  time   = 0;
  for(auto spl : {&posSpline,&targetSpline}) {
    spl->t    = 0;
    spl->dist = 0;
    }
  if(auto camera = Gothic::inst().camera();activeEvents==0) {
    camera->reset();
    camera->setCsEvent(true);
    camera->setMode(Camera::Mode::Normal);
    godMode = Gothic::inst().isGodMode();
    Gothic::inst().setGodMode(true);
    }
  activeEvents++;
  enableTicks();
  }

void CsCamera::onUntrigger(const TriggerEvent& evt) {
  clear();
  }

void CsCamera::clear() {
  if(!active)
    return;
  active = false;
  activeEvents--;
  disableTicks();
  if(auto camera = Gothic::inst().camera();activeEvents==0) {
    camera->setCsEvent(false);
    camera->reset();
    Gothic::inst().setGodMode(godMode);
    }
  }

void CsCamera::tick(uint64_t dt) {
  time += float(dt)/1000.f;

  if(time>duration+delay) {
    clear();
    return;
    }

  if(time>duration)
    return;

  Vec3 cPos,cSpin;
  if(posSpline.size()==1)
    cPos = posSpline[0].c[3];
  else if(posSpline.size()>1)
    cPos = position(posSpline);

  auto camera = Gothic::inst().camera();
  if(targetSpline.size()==0)
    cSpin = cPos - camera->destPosition();
  else if(targetSpline.size()==1)
    cSpin = targetSpline[0].c[3] - cPos;
  else if(targetSpline.size()>1)
    cSpin = position(targetSpline) - cPos;

  camera->setPosition(cPos);
  camera->setSpin(spin(cSpin));
  }

void CsCamera::init(const phoenix::vobs::cs_camera& cam) {
  for(auto spl : {&posSpline,&targetSpline}) {
    uint32_t size = spl->size();
    float    len  = 0;
    for(uint32_t i=0;i+1<size;++i) {
      uint32_t k = i;
      if(spl==&targetSpline)
        k += uint32_t(cam.position_count);
      auto& f   = cam.frames[k];
      auto& kF  = spl->keyframe[i];
      float a1  = (1+f->cam_bias) * (1-f->tension) * (1+f->continuity) * 0.5f;
      float a2  = (1-f->cam_bias) * (1-f->tension) * (1-f->continuity) * 0.5f;
      float a3  = (1+f->cam_bias) * (1-f->tension) * (1-f->continuity) * 0.5f;
      float a4  = (1-f->cam_bias) * (1-f->tension) * (1+f->continuity) * 0.5f;
      Vec3  p0  = i==0 ? kF.c[3] : spl->keyframe[i-1].c[3];
      Vec3  p1  = kF.c[3];
      Vec3  p2  = spl->keyframe[i+1].c[3];
      Vec3  p3  = i+2==size ? kF.c[3] : spl->keyframe[i+2].c[3];
      Vec3  dd  = (p1-p0)*a1 + (p2-p1)*a2;
      Vec3  sd  = (p2-p1)*a3 + (p3-p2)*a4;
      kF.c[0] =  2 * p1 - 2*p2 +   dd + sd;
      kF.c[1] = -3 * p1 + 3*p2 - 2*dd - sd;
      kF.c[2] = std::move(dd);
      len    += kF.arcLength();
      }

    if(size<2)
      continue;
    assert(spl->keyframe.front().time==0);
    assert(spl->keyframe.back().time==duration);
    const float    vSlow  = 0;
    const float    vMean  = len/duration;
          uint32_t start  = spl==&posSpline ? 0 : uint32_t(cam.position_count);
          uint32_t end    = spl==&posSpline ? uint32_t(cam.position_count-1) : uint32_t(cam.frames.size()-1);
          auto     mType0 = cam.frames[start]->motion_type;
          auto     mType1 = cam.frames[end]->motion_type;
          float    d0     = mType0==phoenix::camera_motion::slow ? vSlow : vMean * duration;
          float    d1     = mType1==phoenix::camera_motion::slow ? vSlow : vMean * duration;

    spl->c[0] = - 2*len +     d0 + d1;
    spl->c[1] = + 3*len - 2*d0 - d1;
    spl->c[2] = d0;
    }
  }

 float CsCamera::KeyFrame::arcLength(float t0,float t1) {
  // based on https://www.gnu.org/software/gsl/doc/html/integration.html#qag-adaptive-integration
  auto qGK = [this](float t0, float t1,float& area, float& error) {
    auto f = [this](float t) {
    Vec3 b = c[0]*t*t*3.f + c[1]*t*2.f + c[2];
    return b.length();
    };

    static const float x[7] = {
      0.991455371120812639206854697526329f,
      0.949107912342758524526189684047851f,
      0.864864423359769072789712788640926f,
      0.741531185599394439863864773280788f,
      0.586087235467691130294144838258730f,
      0.405845151377397166906606412076961f,
      0.207784955007898467600689403773245f
      };
    static const float wg[4] = {
      0.129484966168869693270611432679082f,
      0.279705391489276667901467771423780f,
      0.381830050505118944950369775488975f,
      0.417959183673469387755102040816327f
      };
    static const float wgk[8] = {
      0.022935322010529224963732008058970f,
      0.063092092629978553290700663189204f,
      0.104790010322250183839876322541518f,
      0.140653259715525918745189590510238f,
      0.169004726639267902826583426598550f,
      0.190350578064785409913256402421014f,
      0.204432940075298892414161999234649f,
      0.209482141084727828012999174891714f
      };

    float mid     = 0.5f * (t0+t1);
    float len     = 0.5f * (t1-t0);
    float fMid    = f(mid);
    float gauss   = fMid * wg[3];
    float kronrod = fMid * wgk[7];
    for(int i=0;i<3;i++) {
      int   k    = 2*i + 1;
      float xTr  = len * x[k];
      float fSum = f(mid+xTr) + f(mid-xTr);
      gauss   += wg[i]  * fSum;
      kronrod += wgk[k] * fSum;
      }
    for(int k=0;k<8;k+=2) {
      float xTr = len * x[k];
      kronrod += wgk[k] * (f(mid+xTr) + f(mid-xTr));
      };

    area  = kronrod * len;
    error = std::abs(kronrod-gauss) * len;
    };

  const float              tol   = 1e-6f;
        float              area  = 0,  errsum = 0;
        std::vector<float> alist = {t0}, blist  = {t1}, rlist, elist;
        uint32_t           imax  = 0;
        uint32_t           n     = 0;

  qGK(t0,t1,area,errsum);
  if((errsum<=tol) || errsum == 0)
    return area;
  rlist.push_back(area);
  elist.push_back(errsum);

  for(;;) {
    float area1  = 0, area2  = 0;
    float error1 = 0, error2 = 0, error12 = 0;
    float e  = elist[imax];
    float tMid;

    n++;
    t0   = alist[imax];
    t1   = blist[imax];
    tMid = 0.5f * (t0+t1);
    qGK(t0,tMid,area1,error1);
    qGK(tMid,t1,area2,error2);
    error12   = error1 + error2;
    errsum   += (error12-e);
    if(errsum<=tol || (error12>=e && n>10))
      break;
    if(error2>error1) {
      alist[imax] = tMid;
      rlist[imax] = area2;
      elist[imax] = error2;
      alist.push_back(t0);
      blist.push_back(tMid);
      rlist.push_back(area1);
      elist.push_back(error1);
      } else {
      blist[imax] = tMid;
      rlist[imax] = area1;
      elist[imax] = error1;
      alist.push_back(tMid);
      blist.push_back(t1);
      rlist.push_back(area2);
      elist.push_back(error2);
      }
    for(uint32_t j=0;j<elist.size();++j)
      if(elist[j]>elist[imax])
        imax = j;
    }

  area = 0.f;
  for(float r:rlist)
    area += r;
  return area;
  }

Tempest::Vec3 CsCamera::position(CamSpline& spl) {
  float n, lB, uB, tNew;
  float t                = std::modf(spl.t,&n);
  auto  s                = &spl[uint32_t(n)];
  float d                = spl.nextDist(time/duration);

  const float step = 0.005f;
  uB = t + step;
  while(d>s->arcLength(t,uB))
    uB += step;
  lB   = uB - step;
  tNew = 0.5f * (lB+uB);

  for(;;) {
    if(lB>=1) {
      d      -= s->arcLength(t,1);
      spl.t   = std::ceil(spl.t);
      n       = spl.t;
      s       = &spl[uint32_t(n)];
      uB      = step;
      t       = 0;
      assert(uint32_t(n)+1<spl.size());
      while(d>s->arcLength(t,uB))
        uB += step;
      lB   = uB - step;
      tNew = 0.5f * (lB+uB);
      }
    if(tNew==lB || tNew==uB)
      break;
    if(s->arcLength(t,tNew)>d)
      uB = tNew; else
      lB = tNew;
    tNew = 0.5f * (lB+uB);
    }
  // use lB instead of tNew to prevent overshoot
  spl.t = n + lB;
  return s->c[0]*lB*lB*lB + s->c[1]*lB*lB + s->c[2]*lB + s->c[3];
  }

PointF CsCamera::spin(const Tempest::Vec3& d) {
  float k     = 180.f/float(M_PI);
  float spinX = k * std::asin(d.y/d.length());
  float spinY = -90;
  if(d.x!=0.f || d.z!=0.f)
    spinY = 90 + k * std::atan2(d.z,d.x);
  return {-spinX,spinY};
  }

float CsCamera::CamSpline::nextDist(float t) {
  float d = dist;
  dist = c[0]*t*t*t + c[1]*t*t + c[2]*t;
  assert(dist>d);
  return dist - d;
  }
