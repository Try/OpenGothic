#include "camera.h"

#include <limits>

#include <Tempest/Application>
#include <Tempest/Log>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "game/definitions/cameradefinitions.h"
#include "graphics/mesh/animmath.h"
#include "game/serialize.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"
#include "gothic.h"
#include "commandline.h"

#include "utils/string_frm.h"

using namespace Tempest;

static float angleMod(float a) {
  a = std::fmod(a,360.f);
  if(a<-180.f)
    a+=360.f;
  if(a>180.f)
    a-=360.f;
  return a;
  }
/*
static Vec3 angleMod(Vec3 a) {
  a.x = angleMod(a.x);
  a.y = angleMod(a.y);
  a.z = angleMod(a.z);
  return a;
  }*/

static zenkit::Quat fromAngles(Vec3 angles) {
  float roll  = float(angles.x*M_PI)/180.f;
  float yaw   = float(angles.y*M_PI)/180.f;
  float pitch = 0.f;

  float cr = std::cos(roll  * 0.5f);
  float sr = std::sin(roll  * 0.5f);
  float cp = std::cos(pitch * 0.5f);
  float sp = std::sin(pitch * 0.5f);
  float cy = std::cos(yaw   * 0.5f);
  float sy = std::sin(yaw   * 0.5f);

  zenkit::Quat q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;

  return q;
  }

static Vec3 toAngles(zenkit::Quat q) {
  float roll, pitch, yaw;

  // roll (x-axis rotation)
  float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
  float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
  roll = std::atan2(sinr_cosp, cosr_cosp);

  // pitch (y-axis rotation)
  float sinp = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
  float cosp = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
  pitch = 2.f * std::atan2(sinp, cosp) - float(M_PI / 2.0);

  // yaw (z-axis rotation)
  float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
  float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
  yaw = std::atan2(siny_cosp, cosy_cosp);

  return Vec3(roll,yaw,pitch)*float(180.f/M_PI);
  }

const float Camera::minLength = 0.0001f;

Camera::Camera() {
  }

void Camera::reset() {
  reset(Gothic::inst().player());
  }

void Camera::reset(const Npc* pl) {
  const auto& def = cameraDef();

  userRange    = (def.best_range - def.min_range)/(def.max_range - def.min_range);
  state.range  = userRange*(def.max_range-def.min_range)+def.min_range;
  state.target = pl ? pl->cameraBone() : Vec3();

  state.spin.x  = 0;
  state.spin.y  = pl ? pl->rotation() : 0;
  state.spin   += Vec3(def.best_elevation,
                       def.best_azimuth,
                       def.best_rot_z);

  arpg.init     = false;
  inter.target  = state.target;
  tickThirdPerson(-1.f);
  }

void Camera::save(Serialize &s) {
  s.write(state.range, state.spin, state.target);
  s.write(inter.target, inter.rotOffset);
  s.write(origin,angles,veloTrans);
  s.write(userRange);
  }

void Camera::load(Serialize &s, Npc* pl) {
  reset(pl);
  if(s.version()<55)
    return;
  s.read(state.range, state.spin, state.target);
  s.read(inter.target, inter.rotOffset);
  s.read(origin,angles,veloTrans);
  s.read(userRange);
  }

void Camera::changeZoom(int delta) {
  if(isArpgActive()) {
    arpg.rangeDest += (delta>0 ? -100.f : 100.f);
    arpg.rangeDest  = std::clamp(arpg.rangeDest, 500.f, 2200.f);
    return;
    }
  if(fpEnable)
    return;
  if(delta>0)
    userRange-=0.02f; else
    userRange+=0.02f;
  userRange = std::max(0.f,std::min(userRange,1.f));
  }

void Camera::setViewport(uint32_t w, uint32_t h) {
  const float fov = Gothic::options().cameraFov;
  proj.perspective(fov, float(w)/float(h), zNear(), zFar());

  // NOTE: usually depth bouds are from 0.95 to 1.0, resilting into ~805675 discrete values
  static float l = 0.951978f, r = 1;
  auto il = reinterpret_cast<uint32_t&>(l);
  auto ir = reinterpret_cast<uint32_t&>(r);
  auto diff = ir-il;
  (void)diff;

  vpWidth   = w;
  vpHeight  = h;
  depthNear = 0;
  fovNow    = fov;
  }

float Camera::zNear() const {
  static float near = 10.f;
  return near;
  }

float Camera::zFar() const {
  static float far = 100000.0f;
  return far;
  }

void Camera::rotateLeft(uint64_t dt) {
  implMove(KeyEvent::K_Q,dt);
  }

void Camera::rotateRight(uint64_t dt) {
  implMove(KeyEvent::K_E,dt);
  }

void Camera::moveForward(uint64_t dt) {
  implMove(KeyEvent::K_W,dt);
  }

void Camera::moveBack(uint64_t dt) {
  implMove(KeyEvent::K_S,dt);
  }

void Camera::moveLeft(uint64_t dt) {
  implMove(KeyEvent::K_A,dt);
  }

void Camera::moveRight(uint64_t dt) {
  implMove(KeyEvent::K_D,dt);
  }

void Camera::setMode(const Camera::Mode m) {
  if(camMod==m)
    return;

  auto isRegular = [](const Camera::Mode m){
    return m==Normal || m==Inventory || m==Melee || m==Ranged || m==Magic;
    };

  const bool reset = !(isRegular(camMod) && isRegular(m));
  if(camMod==Mode::Cutscene) {
    state.spin   = angles;
    state.target = origin;
    inter.target = origin;
    }

  camMod = m;

  if(camMarvinMod==M_Free || camMarvinMod==M_Freeze)
    return;

  if(camMod==Camera::FirstPerson) {
    if(auto pl = Gothic::inst().player()) {
      state.spin = Vec3(0, pl->rotation(), 0);
      }
    return;
    }

  const auto& def = cameraDef();

  if(reset) {
    state.range = def.best_range;
    if(def.max_range > def.min_range + 0.01f)
      userRange = (def.best_range - def.min_range)/(def.max_range - def.min_range); else
      userRange = 0;
    state.spin = Vec3(0);
    }

  if(auto pl = Gothic::inst().player()) {
    state.spin = Vec3(0, pl->rotation(), 0);
    }

  auto rotBest = Vec3(def.best_elevation,
                      def.best_azimuth,
                      def.best_rot_z);
  state.spin += rotBest;
  }

void Camera::setMarvinMode(Camera::MarvinMode nextMod) {
  if(camMarvinMod==nextMod)
    return;

  if(nextMod==M_Pinned) {
    const auto pl = Gothic::inst().player();

    auto      offset = origin;
    Matrix4x4 rotMat = pl!=nullptr ? pl->cameraMatrix(false) : Matrix4x4::mkIdentity();
    rotMat.inverse();
    rotMat.project(offset);

    pin.origin = offset;
    pin.spin   = angles;
    }
  else if(nextMod==M_Free) {
    state.spin   = angles;
    state.target = origin;
    inter.target = origin;
    }
  camMarvinMod = nextMod;
  }

bool Camera::isMarvin() const {
  return camMarvinMod!=M_Normal;
  }

bool Camera::isFree() const {
  return camMarvinMod==M_Free;
  }

bool Camera::isInWater() const {
  return inWater;
  }

bool Camera::isCutscene() const {
  return camMod==Camera::Mode::Cutscene;
  }

void Camera::setToggleEnable(bool e) {
  tgEnable = e;
  }

bool Camera::isToggleEnabled() const {
  return tgEnable;
  }

void Camera::setInertiaTargetEnable(bool e) {
  inertiaTarget = e;
  }

bool Camera::isInertiaTargetEnabled() const {
  return inertiaTarget;
  }

void Camera::setFirstPerson(bool fp) {
  fpEnable = fp;
  }

bool Camera::isFirstPerson() const {
  return fpEnable;
  }

void Camera::setLookBack(bool lb) {
  if(lbEnable==lb)
    return;
  lbEnable = lb;
  }

void Camera::toggleDebug() {
  dbg = !dbg;
  }

void Camera::setSpin(const PointF &p) {
  state.spin = /*angleMod*/(Vec3(p.x,p.y,0));
  }

void Camera::setTarget(const Tempest::Vec3& pos) {
  state.target = pos;
  }

void Camera::setAngles(const Tempest::PointF& p) {
  angles = /*angleMod*/(Vec3(p.x,p.y,0));
  }

void Camera::setPosition(const Tempest::Vec3& pos) {
  origin = pos;
  }

void Camera::setDialogDistance(float d) {
  dlgRange = d;
  }

void Camera::onRotateMouse(const PointF& dpos) {
  state.spin.x += dpos.x;
  state.spin.y += dpos.y;
  }

Matrix4x4 Camera::projective() const {
  auto ret = proj;
  if(auto w = Gothic::inst().world())
    w->globalFx()->morph(ret);
  return ret;
  }

Matrix4x4 Camera::viewShadowVsm(const Tempest::Vec3& ldir) const {
  return mkViewShadowVsm(inter.target,ldir);
  }

Matrix4x4 Camera::viewShadowVsmLwc(const Tempest::Vec3& ldir) const {
  return mkViewShadowVsm(inter.target-(origin+shake),ldir);
  }

Matrix4x4 Camera::mkViewShadowVsm(const Vec3& cameraPos, const Vec3& ldir) const {
  float smWidth    = 1024; // ~4 pixels per santimeter
  float smDepth    = 10*5120;

  float smWidthInv = 1.f/smWidth;
  float zScale     = 1.f/smDepth;

  auto up = std::abs(ldir.z)<0.999 ? Vec3(0,0,1) : Vec3(1,0,0);
  auto z  = ldir;
  auto x  = Vec3::normalize(Vec3::crossProduct(z, up));
  auto y  = Vec3::crossProduct(x, z);

  auto view = Matrix4x4 {
           x.x, y.x, z.x, 0,
           x.y, y.y, z.y, 0,
           x.z, y.z, z.z, 0,
             0,   0,   0, 1
         };
  view.transpose();
  {
    Matrix4x4 scale;
    scale.identity();
    scale.scale(smWidthInv, smWidthInv, zScale);
    scale.mul(view);
    view = scale;
  }
  view.translate(-cameraPos);

  {
    Matrix4x4 proj;
    proj.identity();
    proj.translate(0.f, 0.f, 0.5f);
    proj.mul(view);
    view = proj;
  }

  return view;
  }

Matrix4x4 Camera::viewShadow(const Vec3& lightDir, size_t layer) const {
  auto  vp       = viewProj();
  float rotation = (angles.y-90);
  // if(layer==0)
  //   return viewShadowVsm(cameraPos,rotation,vp,lightDir);
  return mkViewShadow(inter.target,rotation,vp,lightDir,layer);
  }

Matrix4x4 Camera::viewShadowLwc(const Tempest::Vec3& lightDir, size_t layer) const {
  auto  vp       = viewProjLwc();
  float rotation = (angles.y-90);
  // if(layer==0)
  //   return viewShadowVsm(cameraPos-origin,rotation,vp,lightDir);
  return mkViewShadow(inter.target-origin,rotation,vp,lightDir,layer);
  }

Matrix4x4 Camera::mkViewShadow(const Vec3& cameraPos, float rotation, const Tempest::Matrix4x4& viewProj, const Vec3& lightDir, size_t layer) const {
  Vec3  ldir     = lightDir;
  float eps      = 0.005f;

  if(std::abs(ldir.y)<eps) {
    float k = (1.f-eps*eps)/std::sqrt(ldir.x*ldir.x + ldir.z*ldir.z);
    ldir.y = (ldir.y<0) ? -eps : eps;
    ldir.x *= k;
    ldir.z *= k;
    }

  Vec3 center = cameraPos;
  auto vp = viewProj;
  vp.project(center);

  vp.inverse();
  Vec3 l = {-1,center.y,center.z}, r = {1,center.y,center.z};
  vp.project(l);
  vp.project(r);

  float smWidth    = 0;
  float smDepth    = 5120*5;
  switch(layer) {
    case 0:
      smWidth    = (r-l).length();
      smWidth    = std::max(smWidth,1024.f); // ~4 pixels per santimeter
      smDepth    = 5120;
      break;
    case 1:
      smWidth    = 5120;
      smDepth    = 5120*5;
      break;
    };

  float smWidthInv = 1.f/smWidth;
  float zScale     = 1.f/smDepth;

  Matrix4x4 view;
  view.identity();
  view.rotate(-90, 1, 0, 0);     // +Y -> -Z
  view.rotate(rotation, 0, 1, 0);
  view.scale(smWidthInv, zScale, smWidthInv);
  view.translate(cameraPos);
  view.scale(-1,-1,-1);

  // sun direction
  if(ldir.y!=0.f) {
    float lx = ldir.x/std::abs(ldir.y);
    float lz = ldir.z/std::abs(ldir.y);

    const float ang = -rotation*float(M_PI)/180.f;
    const float c   = std::cos(ang), s = std::sin(ang);

    float dx = lx*c-lz*s;
    float dz = lx*s+lz*c;

    view.set(1,0, dx*smWidthInv);
    view.set(1,1, dz*smWidthInv);
    }

  // stretch shadowmap on light dir
  if(ldir.y!=0.f) {
    // stretch view to camera
    float r0 = std::fmod(rotation, 360.f);
    float r  = std::fmod(std::atan2(ldir.z,ldir.x)*180.f/float(M_PI), 360.f);
    r -= r0;

    float s = std::abs(ldir.y);
    Matrix4x4 proj;
    proj.identity();
    proj.rotate( r, 0, 0, 1);
    proj.translate(-0.5f,0,0);
    proj.scale(s, 1, 1);
    //proj.scale(0.1f, 1, 1);
    proj.translate(0.5f,0,0);
    proj.rotate(-r, 0, 0, 1);
    proj.mul(view);
    view = proj;
    }

  if(layer>0) {
    // projective shadowmap
    Tempest::Matrix4x4 proj;
    proj.identity();

    static float k = -0.4f;
    proj.set(1,3, k);
    proj.mul(view);
    view = proj;
    }

  auto inv = view;
  inv.inverse();
  Vec3 mid = {};
  inv.project(mid);
  view.translate(mid-cameraPos);

  {
    Matrix4x4 proj;
    proj.identity();
    switch(layer) {
      case 0:
        proj.translate(0.f, 0.8f, 0.5f);
        break;
      case 1: {
        proj.translate(0.f, 0.5f, 0.5f);
        break;
        }
      }

    proj.mul(view);
    view = proj;
    }

  return view;
  }

Camera::ListenerPos Camera::listenerPosition() const {
  auto vp = viewProj();
  vp.inverse();

  ListenerPos pos;
  pos.up    = Vec3(0,1,0);
  pos.front = Vec3(0,0,1);
  pos.pos   = Vec3(0,0,0);
  vp.project(pos.up);
  vp.project(pos.front);
  vp.project(pos.pos);

  pos.up    = Vec3::normalize(pos.up    - pos.pos);
  pos.front = Vec3::normalize(pos.front - pos.pos);
  return pos;
  }

const zenkit::ICamera& Camera::cameraDef() const {
  auto& camd = Gothic::cameraDef();
  if(camMod==Dialog)
    return camd.dialogCam();
  if(lbEnable)
    return camd.backCam();
  if(fpEnable && (camMod==Normal || camMod==Melee))
    return camd.fpCam();
  if(camMod==Normal) {
    return camd.stdCam();
    }
  if(camMod==Inventory)
    return camd.inventoryCam();
  if(camMod==Melee)
    return camd.meleeCam();
  if(camMod==Ranged)
    return camd.rangeCam();
  if(camMod==Magic)
    return camd.mageCam();
  if(camMod==Swim)
    return camd.swimCam();
  if(camMod==Dive)
    return camd.diveCam();
  if(camMod==Fall)
    return camd.fallCam();
  if(camMod==Mobsi) {
    std::string_view tag = "", pos = "";
    if(auto pl = Gothic::inst().player())
      if(auto inter = pl->interactive()) {
        tag = inter->schemeName();
        pos = inter->posSchemeName();
        }
    return camd.mobsiCam(tag,pos);
    }
  if(camMod==Death)
    return camd.deathCam();
  return camd.stdCam();
  }

void Camera::implMove(Tempest::Event::KeyType key, uint64_t dt) {
  float dpos      = float(dt);
  float dRot      = dpos/15.f;
  float k         = float(M_PI/180.0);
  float s         = std::sin(-angles.y*k), c=std::cos(-angles.y*k);
  float sx        = std::sin(angles.x*k);
  float cx        = std::cos(angles.x*k);

  if(key==KeyEvent::K_A) {
    origin.x += dpos*s;
    origin.z += dpos*c;
    }
  if(key==KeyEvent::K_D) {
    origin.x -= dpos*s;
    origin.z -= dpos*c;
    }
  if(key==KeyEvent::K_W) {
    origin.x += dpos*c*cx;
    origin.z -= dpos*s*cx;
    origin.y -= dpos*sx;
    }
  if(key==KeyEvent::K_S) {
    origin.x -= dpos*c*cx;
    origin.z += dpos*s*cx;
    origin.y += dpos*sx;
    }
  if(key==KeyEvent::K_Q)
    state.spin.y += dRot;
  if(key==KeyEvent::K_E)
    state.spin.y -= dRot;
  }

Vec3 Camera::followTarget(Vec3 pos, Vec3 dest, float dtF) {
  auto dp  = (dest-pos);
  auto len = dp.length();

  if(dtF<=0.f) {
    return dest;
    }

  if(len<=minLength) {
    return dest;
    }

  // no idea how it trully meant to work in vanilla game
  if(inertiaTarget) {
    static float mul11 = 8.f;
    static float mul12 = 2.f;

    veloTrans = veloTrans+(dp-veloTrans)*std::min(1.f, mul11*dtF);
    pos += veloTrans*std::min(1.f, mul12*dtF);
    return pos;
    } else {
    static float mul21 = 3.f;

    veloTrans = dp;
    pos += veloTrans*std::min(1.f, mul21*dtF);
    return pos;
    }

  /*
    {
    auto diff = dp*k;
    float speed = diff.length()/dtF;
    static float prevSpeed = 0;

    if(false && speed > 1.f)
      Log::i("speed: ", speed, "delta: ", std::abs(prevSpeed-speed));
    prevSpeed = speed;
    }
  */

  return pos;
  }

Tempest::Vec3 Camera::followTrans(Vec3 pos, Tempest::Vec3 dest, float dtF, float velo) {
  if(dtF<0)
    return dest;

  /*
  static uint64_t time = 0;
  if((dest-pos).length()<1 || velo==0) {
    auto tx = Tempest::Application::tickCount();
    if(velo>0)
      Log::d("time = ", tx-time);
    time = tx;
    }
  */
  static float k = 0.25f;
  return pos + (dest-pos)*std::min(1.f, k*velo*dtF);
  }

Tempest::Vec3 Camera::followRot(Vec3 spin, Tempest::Vec3 dest, float dtF, float velo) {
  if(dtF<0.f)
    return dest;

#if 1
  const zenkit::Quat sx = fromAngles(spin);
  const zenkit::Quat dx = fromAngles(dest);

  const zenkit::Quat s  = slerp(sx, dx, std::min(1.f, dtF*velo));
  return toAngles(s);
#else
  const zenkit::Quat d = fromAngles(dest);
  return toAngles(d);
#endif
  }

void Camera::followAng(Vec3& spin, Vec3 dest, float dtF) {
  const auto& def = cameraDef();
  followAng(spin.x, dest.x, def.velo_rot, dtF);
  followAng(spin.y, dest.y, def.velo_rot, dtF);
  }

void Camera::followAng(float& ang, float dest, float speed, float dtF) {
  float da    = angleMod(dest-ang);
  float shift = da*std::min(1.f, speed*dtF);
  if(std::abs(da)<=0.0001f || dtF<0.f) {
    ang = dest;
    return;
    }
  ang += shift;
  }

void Camera::tick(uint64_t dt) {
  if(Gothic::inst().isPause() || (camMarvinMod==M_Freeze && camMod!=Dialog))
    return;

  if(isCutscene()) {
    return; // handle pass thru water ?
    }

  const float dtF = float(dt)/1000.f;

  if(camMod!=Dialog) {
    // gothic-diablo: the camera goes back up from the last shot; the next talk starts with the way down again
    if(arpgDlg.cutDone && isArpgActive()) {
      arpgDlg.exitOn   = true;
      arpgDlg.exitT    = 0;
      arpgDlg.exitFrom = origin;
      arpgDlg.exitLook = arpgDlg.look;
      }
    arpgDlg.t        = 0;
    arpgDlg.cutDone  = false;
    arpgDlg.keepView = false;
    }
  if(!isArpgActive())
    arpgDlg.exitOn = false;
  {
    const float dst = isArpgDialogShot() ? 1.f : 0.f;
    arpgDlg.dof += std::clamp(dst-arpgDlg.dof, -4.f*dtF, 3.f*dtF);
  }

  {
    const auto& def = cameraDef();
    state.range = def.min_range + (def.max_range-def.min_range)*userRange;
  }

  // normalize angles in -180..180 range, for convinience
  // dst.spin = angleMod(dst.spin);
  // src.spin = angleMod(src.spin);

  auto prev = origin;

  switch (camMarvinMod) {
    case M_Normal: {
      if(camMod==Camera::FirstPerson) {
        tickFirstPerson(dtF);
        }
      else {
        tickThirdPerson(dtF);
        }
      break;
      }
    case M_Freeze: {
      // nope
      break;
      }
    case M_Free: {
      inter.target = origin;
      angles       = followRot(angles, state.spin, dtF, 10.f);
      break;
      }
    case M_Pinned: {
      const auto pl     = Gothic::inst().player();
      const auto rotMat = pl!=nullptr ? pl->cameraMatrix(false) : Matrix4x4::mkIdentity();

      auto offset = pin.origin;
      rotMat.project(offset);
      origin       = offset;
      inter.target = offset;
      angles       = pin.spin;
      break;
      }
    }

  if(camMarvinMod==M_Normal && camMod!=FirstPerson && isArpgEnabled())
    tickArpgBook(dtF);
  updateArpgView(dtF);
  arpgDlg.on = false;

  {
    // gothic-diablo: the dialog shots are filmed with a narrower lens (and the way back up widens it again)
    float fov = isArpgDialogShot() ? 50.f : Gothic::options().cameraFov;
    if(arpgDlg.exitOn) {
      const float s = std::clamp(arpgDlg.exitT/0.8f, 0.f, 1.f);
      fov = 50.f + (fov-50.f)*(s*s*(3.f-2.f*s));
      }
    if(fov!=fovNow && vpWidth>0 && vpHeight>0) {
      proj.perspective(fov, float(vpWidth)/float(vpHeight), zNear(), zFar());
      fovNow = fov;
      }
  }

  if(auto w = Gothic::inst().world()) {
    Vec3 sh;
    w->globalFx()->shake(sh);
    shake = shake + (sh-shake)*std::min(1.f, dtF*1000.f);
    }

  {
    //NOTE: in vanilla output of velocity is garbage in general, but zero for static camera
    targetVelo = (origin - prev).length()/dtF;
  }

  auto world = Gothic::inst().world();
  if(world!=nullptr) {
    auto  pl     = isFree() ? nullptr : world->player();
    auto& physic = *world->physic();

    if(pl!=nullptr && pl->isSwim()) {
      inWater = (angles.x < -8.f);
      }
    else if(pl!=nullptr && (pl->isInAir() || pl->isJump()) && !pl->isDead()) {
      // NOTE: not quite correct
      inWater = physic.cameraRay(inter.target, origin).waterCol % 2;
      }
    else {
      // NOTE: find a way to avoid persistent tracking
      inWater = inWater ^ (physic.cameraRay(prev, origin).waterCol % 2);
      }
    }
  }

void Camera::tickFirstPerson(float /*dtF*/) {
  const auto pl     = Gothic::inst().player();
  const auto rotMat = pl!=nullptr ? pl->cameraMatrix(true) : Matrix4x4::mkIdentity();

  state.spin   = clampRotation(state.spin);
  inter.target = state.target;

  Vec3 offset = {0,0,20};
  rotMat.project(offset);
  origin = offset;
  angles = state.spin;
  }

void Camera::tickThirdPerson(float dtF) {
  if(isArpgActive()) {
    tickArpg(dtF);
    return;
    }
  if(camMod==Dialog && isArpgEnabled() && camMarvinMod==M_Normal) {
    tickArpgDialog(dtF);
    return;
    }

  const auto& def = cameraDef();

  auto mkRotMatrix = [this](Vec3 spin){
    auto view = Camera::mkRotation(spin);
    view.inverse();
    return view;
    };

  auto  targetOffset = Vec3(def.target_offset_x,
                            def.target_offset_y,
                            def.target_offset_z);
  auto  rotOffsetDef = Vec3(def.rot_offset_x,
                            def.rot_offset_y,
                            def.rot_offset_z);
  auto  range        = (camMod==Dialog) ? dlgRange : state.range*100.f;

  if(camMod==Dialog) {
    // TODO: DialogCams.zen
    inter.target    = state.target;
    inter.rotOffset = Vec3(0);
    }

  if(camMod!=Dialog) {
    state.spin      = clampRotation(state.spin);
    inter.rotOffset = followRot(inter.rotOffset, rotOffsetDef, dtF, def.velo_rot);
    }

  const auto rotOffsetMat = mkRotMatrix(state.spin);
  if(camMod!=Dialog) {
    rotOffsetMat.project(targetOffset);
    // vanilla clamps target-offset according to collision
    if(def.collision!=0) {
      targetOffset = calcCameraColision(state.target, targetOffset);
      }
    // and has leash-like follow for target+offset
    // ignores def.translate
    inter.target = followTarget(inter.target, state.target+targetOffset, dtF);
    }

  auto dir = Vec3{0,0,1};
  rotOffsetMat.project(dir);

  if(true && def.collision!=0) {
    auto rotation = calcLookAtAngles(inter.target + dir*range, inter.target, inter.rotOffset, state.spin);
    // testd in marvin: collision is calculated from offseted 'target', not from npc
    range = calcCameraColision(inter.target, dir, rotation, range);
    // NOTE: with range < 80, camera gradually moves up in vanilla
    if(range<80.f) {
      range      = 150; // also collision?!
      rotation.x = 80;
      rotation.y = state.spin.y;
      const auto rotOffsetMat = mkRotMatrix(rotation);
      dir = Vec3{0,0,1};
      rotOffsetMat.project(dir);
      }
    }

  if(def.translate!=0) {
    origin = followTrans(origin, inter.target + dir*range, (camMod!=Dialog ? dtF : -1.f), def.velo_trans);
    }

  angles = calcLookAtAngles(origin, inter.target, inter.rotOffset, state.spin);

  static bool dbg = false;
  if(dbg) {
    origin = state.target + targetOffset + dir*range;
    angles = calcLookAtAngles(origin, state.target, inter.rotOffset, state.spin);
    }
  }

float Camera::calcCameraColision(const Tempest::Vec3& target, const Tempest::Vec3& dir, const Tempest::Vec3& angles, float range) const {
  //static float minDist = 20;
  static float padding = 25;
  static int n = 1, nn=1;

  auto world = Gothic::inst().world();
  if(world==nullptr)
    return range;

  const auto origin = target + dir*range;

  Matrix4x4 vinv = projective();
  vinv.mul(mkView(origin,angles));
  vinv.inverse();

  auto& physic = *world->physic();
  auto  dview  = (origin - target);

  raysCasted = 0;
  float distM = range;
  for(int i=-1;i<=n;++i)
    for(int r=-n;r<=n;++r) {
      raysCasted++;
      float u = float(i)/float(nn),v = float(r)/float(nn);
      Tempest::Vec3 r1 = {u,v,depthNear};
      vinv.project(r1);
      auto dr = (r1 - target);
      dr = dr * (range+padding) / (dr.length()+0.00001f);

      auto rc = physic.ray(target, target+dr);
      if(!rc.hasCol)
        continue;

      auto  tr    = (rc.v - target);
      float dist1 = Vec3::dotProduct(dview,tr)/range;

      dist1 = std::max<float>(dist1-padding, 0);
      if(dist1<distM)
        distM = dist1;
      }

  return distM;
  }

Vec3 Camera::calcLookAtAngles(const Tempest::Vec3& origin, const Tempest::Vec3& target, const Vec3& rotOffset, const Vec3& defSpin) const {
  auto  sXZ   = (target - origin);

  float lenXZ = Vec2(sXZ.x,sXZ.z).length();
  float y0    = std::atan2(sXZ.z, sXZ.x)*180.f/float(M_PI);
  float x0    = std::atan2(sXZ.y, lenXZ)*180.f/float(M_PI);

  if(lenXZ < 4.f)
    y0 = defSpin.y;

  return Vec3(-x0,y0,0) - rotOffset;
  }

Vec3 Camera::calcCameraColision(const Tempest::Vec3& from, const Tempest::Vec3& dir) const {
  static float padding = 25;

  auto world = Gothic::inst().world();
  if(world==nullptr)
    return dir;

  const auto& physic = *world->physic();
  const auto  len    = dir.length() + padding;
  const auto  nrm    = Vec3::normalize(dir);

  auto rc = physic.ray(from, from+nrm*len);
  if(!rc.hasCol)
    return dir;

  return nrm*std::max(len*rc.hitFraction-padding, 0.f);
  }

Vec3 Camera::clampRotation(Tempest::Vec3 spin) {
  //NOTE: min elevation is zero for nomal camera. assume that it's ignored by vanilla
  float       maxElev = +85;
  float       minElev = -60;
  float       maxAzim = +180;
  float       minAzim = -180;

  const auto pl = Gothic::inst().player();
  if(pl==nullptr)
    return spin;

  const auto plSpin = (Vec3{0,  pl->rotation(), 0});
  spin = spin - plSpin;

  spin.x = std::clamp(spin.x, minElev, maxElev);
  spin.y = std::clamp(spin.y, minAzim, maxAzim);
  return (spin + plSpin);
  }

Matrix4x4 Camera::mkView(const Vec3& pos, const Vec3& spin) const {
  auto view = Matrix4x4::mkIdentity();
  view.scale(-1,-1,-1);

  view.mul(mkRotation(spin));
  view.translate(-pos);

  return view;
  }

Matrix4x4 Camera::mkRotation(const Vec3& spin) const {
  auto view = Matrix4x4::mkIdentity();
  view.rotateOX(spin.x);
  view.rotateOY(spin.y);
  view.rotateOZ(spin.z);
  view.rotateOY(90); // X forward
  return view;
  }

void Camera::debugDraw(DbgPainter& p) {
  if(!dbg)
    return;

  p.setPen(Color(0,1,0));
  p.drawLine(state.target, inter.target);
  p.drawLine(inter.target, origin);

  if(auto pl = Gothic::inst().player()) {
    float a  = pl->rotationRad()+float(M_PI*0.5);
    float c  = std::cos(a), s = std::sin(a);
    auto  ln = Vec3(c,0,s)*25.f;
    p.drawLine(inter.target-ln, inter.target+ln);
    }

  auto& fnt = Resources::font(1.0);
  int   y   = 300+fnt.pixelSize();

  string_frm buf("RaysCasted: ",raysCasted);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("PlayerPos : ",state.target.x, ' ', state.target.y, ' ', state.target.z);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("targetVelo : ",targetVelo);
  p.drawText(8,y,buf); y += fnt.pixelSize()*4;

  buf = string_frm("Range To Player : ", (inter.target-origin).length());
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("Azimuth : ", angleMod(angles.y-state.spin.y));
  p.drawText(8,y,buf); y += fnt.pixelSize();
  buf = string_frm("Elevation : ", inter.rotOffset.x+angles.x);
  p.drawText(8,y,buf); y += fnt.pixelSize();
  }

float Camera::azimuth() const {
  return angleMod(state.spin.y-angles.y);
  }

PointF Camera::spin() const {
  return PointF(state.spin.x,state.spin.y);
  }

Vec3 Camera::destTarget() const {
  return state.target;
  }

Matrix4x4 Camera::viewProj() const {
  Matrix4x4 ret=projective();
  ret.mul(view());
  return ret;
  }

Matrix4x4 Camera::view() const {
  return mkView(origin+shake, angles);
  }

Matrix4x4 Camera::viewLwc() const {
  return mkView(Vec3(0), angles);
  }

Matrix4x4 Camera::viewProjLwc() const {
  Matrix4x4 ret=projective();
  ret.mul(viewLwc());
  return ret;
  }

// gothic-diablo: isometric ARPG camera

bool Camera::isArpgEnabled() {
  return CommandLine::inst().isArpgMode();
  }

bool Camera::isArpgActive() const {
  if(!isArpgEnabled() || camMarvinMod!=M_Normal)
    return false;
  switch(camMod) {
    case Normal:
    case Inventory:
    case Melee:
    case Ranged:
    case Magic:
    case Mobsi:
    case Swim:
    case Fall:
      return true;
    case Dialog:
    case Death:
    case Dive:
    case Cutscene:
    case FirstPerson:
      return false;
    }
  return false;
  }

void Camera::arpgRotate(float deltaDeg) {
  arpg.yawDest += deltaDeg;
  }

void Camera::setArpgPortrait(bool on) {
  if(arpg.portraitOn==on)
    return;
  arpg.portraitOn = on;
  if(on) {
    arpg.yawSaved = arpg.yawDest;
    if(auto pl = Gothic::inst().player())
      arpg.yawDest = pl->rotation()+180.f; // in front of the hero, looking at his face
    } else {
    arpg.yawDest = arpg.yawSaved;
    }
  }

void Camera::tickArpg(float dtF) {
  if(!arpg.init) {
    const auto pl = Gothic::inst().player();
    arpg.yaw     = (pl!=nullptr ? pl->rotation() : state.spin.y);
    arpg.yawDest = arpg.yaw;
    arpg.range   = arpg.rangeDest;
    arpg.init    = true;
    }

  if(dtF<0.f) {
    arpg.yaw   = arpg.yawDest;
    arpg.range = arpg.rangeDest;
    } else {
    followAng(arpg.yaw, arpg.yawDest, 6.f, dtF);
    arpg.range += (arpg.rangeDest-arpg.range)*std::min(1.f, 6.f*dtF);
    }

  // portrait (equipment / character window): low and close, in front of the hero
  {
    const float dst = arpg.portraitOn ? 1.f : 0.f;
    if(dtF<0.f)
      arpg.portrait = dst; else
      arpg.portrait += std::clamp(dst-arpg.portrait, -2.5f*dtF, 2.5f*dtF);
  }
  const float pk    = arpg.portrait*arpg.portrait*(3.f-2.f*arpg.portrait); // smoothstep
  const float elev  = arpg.elevation + (8.f-arpg.elevation)*pk;
  const float range = arpg.range     + (290.f-arpg.range)*pk;

  const Vec3 spin = Vec3(elev, arpg.yaw, 0);
  state.spin      = spin;
  inter.rotOffset = Vec3(0);
  inter.target    = followTarget(inter.target, state.target, dtF);

  Vec3 target = inter.target;
  if(pk>0.f) {
    if(auto pl = Gothic::inst().player()) {
      const Vec3 body = pl->position()+Vec3(0,100.f,0); // the whole hero in the picture
      target = target + (body-target)*pk;
      }
    }

  // direction from target to camera, same math as in tickThirdPerson
  auto rot = mkRotation(spin);
  rot.inverse();
  Vec3 dir = {0,0,1};
  rot.project(dir);

  // no collision: geometry between camera and player is hidden in shaders instead
  origin      = target + dir*range;
  angles      = calcLookAtAngles(origin, target, Vec3(0), spin);
  arpgLookNow = target;

  // after a talk: from the last shot back up to this view
  if(arpgDlg.exitOn) {
    if(dtF>0.f)
      arpgDlg.exitT += dtF;
    const float s = std::clamp(arpgDlg.exitT/0.8f, 0.f, 1.f);
    const float e = s*s*(3.f-2.f*s);
    const Vec3  o = arpgDlg.exitFrom + (origin-arpgDlg.exitFrom)*e;
    const Vec3  l = arpgDlg.exitLook + (target-arpgDlg.exitLook)*e;
    origin      = o;
    angles      = calcLookAtAngles(origin, l, Vec3(0), spin);
    arpgLookNow = l;
    if(s>=1.f)
      arpgDlg.exitOn = false;
    }
  }

void Camera::tickArpgDialog(float dtF) {
  auto        world = Gothic::inst().world();
  const auto& in    = arpgDlg.in;
  const bool  talk  = arpgDlg.on && world!=nullptr && in.pl!=nullptr && in.other!=nullptr && in.pl!=in.other;
  if(!talk) {
    // workbench menus and the like (the hero alone): the ARPG view stays
    arpgDlg.keepView = true;
    tickArpg(dtF);
    return;
    }

  if(arpgDlg.t==0.f)
    arpgDlg.viewFrom = origin;
  if(dtF>0.f)
    arpgDlg.t += dtF;

  static constexpr float ZoomTime = 0.45f;
  if(!arpgDlg.cutDone && arpgDlg.t<ZoomTime) {
    // the ARPG camera comes down towards the two heads ...
    arpgDlg.keepView = true;
    tickArpg(dtF);
    const float s    = std::clamp(arpgDlg.t/ZoomTime, 0.f, 1.f);
    const float k    = s*s*(3.f-2.f*s);
    const Vec3  mid  = (in.pl->mapHeadBone()+in.other->mapHeadBone())*0.5f;
    const Vec3  look = inter.target + (mid-inter.target)*k;
    origin      = origin + (mid-origin)*(0.55f*k);
    angles      = calcLookAtAngles(origin, look, Vec3(0), state.spin);
    arpgLookNow = look;
    return;
    }

  // ... and cuts to the shots of the director; the cut hides that the hero is put in his place
  if(!arpgDlg.cutDone) {
    arpgDlg.cutDone = true;
    arpgDlg.exitOn  = false;
    arpgDir.reset();
    arpgPlaceHero(in);
    }
  arpgDlg.keepView = false;
  ArpgDialogCam::Input din = in;
  din.viewFrom = arpgDlg.viewFrom;
  const auto shot = arpgDir.tick(std::max(0.f, dtF), din, *world);
  origin        = shot.origin;
  arpgDlg.look  = shot.target;
  arpgDlg.focus = shot.focus;
  angles        = calcLookAtAngles(origin, shot.target, Vec3(0), state.spin);
  arpgLookNow   = shot.target;
  }

void Camera::tickArpgBook(float dtF) {
  // trade / chest / looting: the other one (or the thing) in the gap between the two pages of the book;
  // the camera stands at the hero's side of him, turned away from the line between them so far, that the hero
  // is behind a page: a trader from the front and low (like the hero's own portrait), a chest or a body from above
  auto&      b     = arpgBook;
  const auto pl    = Gothic::inst().player();
  const auto world = Gothic::inst().world();
  const bool on    = b.in.on && pl!=nullptr && world!=nullptr;
  if(on && b.k<=0.f)
    b.side = 0; // a new book: the side is picked again
  const float dst = on ? 1.f : 0.f;
  if(dtF<0.f)
    b.k = dst; else
    b.k += std::clamp(dst-b.k, -2.5f*dtF, 2.5f*dtF);
  if(b.k<=0.f)
    return;

  if(on) {
    const Vec3 look = b.in.subject;
    Vec3 a = pl->position()-look;
    a.y = 0;
    if(a.length()<1.f) {
      a   = origin-look;
      a.y = 0;
      }
    a = (a.length()<1.f) ? Vec3(1,0,0) : a/a.length();

    const float turn  = (b.in.standing ? 42.f : 65.f)*float(M_PI)/180.f;
    const float elev  = (b.in.standing ?  8.f : 32.f)*float(M_PI)/180.f;
    const float range =  b.in.standing ? 290.f : 240.f;
    auto dirFor = [&](float sd) {
      const float c = std::cos(sd*turn), sn = std::sin(sd*turn);
      const Vec3  d = Vec3(a.x*c - a.z*sn, 0, a.x*sn + a.z*c);
      return Vec3(d.x*std::cos(elev), std::sin(elev), d.z*std::cos(elev));
      };
    auto& physic = *world->physic();
    auto  room   = [&](const Vec3& dir) {
      // from the subject outwards: walls behind the camera pull it in
      const auto r = physic.ray(look, look+dir*range);
      if(!r.hasCol)
        return range;
      return std::max(60.f, (r.v-look).length()-20.f);
      };
    if(b.side==0.f) {
      // the side with more room; about the same: the side the camera already is on (smaller jump)
      const float rp = room(dirFor( 1.f));
      const float rm = room(dirFor(-1.f));
      if(std::abs(rp-rm)>30.f) {
        b.side = (rp>rm) ? 1.f : -1.f;
        } else {
        const Vec3 cur = origin-look;
        b.side = (a.x*cur.z - a.z*cur.x >= 0.f) ? 1.f : -1.f;
        }
      }
    const Vec3  dir = dirFor(b.side);
    const float len = room(dir);
    b.origin = look + dir*len;
    b.look   = look;
    b.focus  = len;
    }

  const float e = b.k*b.k*(3.f-2.f*b.k);
  origin = origin + (b.origin-origin)*e;
  const Vec3 l = arpgLookNow + (b.look-arpgLookNow)*e;
  angles = calcLookAtAngles(origin, l, Vec3(0), state.spin);
  if(isArpgDialogShot()) {
    // trade from a dialog: the face light and the depth of field follow
    arpgDlg.look  = l;
    arpgDlg.focus = arpgDlg.focus + (b.focus-arpgDlg.focus)*e;
    }
  }

void Camera::arpgPlaceHero(const ArpgDialogCam::Input& in) {
  // the hero turns to the one he talks with and stands at the distance of a talk (1.3 - 1.7 m)
  auto hero = Gothic::inst().player();
  if(hero==nullptr || hero!=in.pl || in.other==nullptr || hero==in.other)
    return;
  if(hero->interactive()!=nullptr || hero->bodyStateMasked()!=BS_STAND)
    return;
  Vec3 d = in.other->position()-hero->position();
  d.y = 0;
  const float l = d.length();
  if(l<1.f)
    return;
  const Vec3  dir  = d/l;
  const float want = std::clamp(l, 130.f, 170.f);
  if(std::abs(l-want)>5.f) {
    const Vec3 dp = dir*(l-want);
    if(!hero->tryMove(dp))
      hero->tryMove(dp*0.5f);
    }
  hero->setDirection(in.other->position()-hero->position());
  }

bool Camera::arpgDialogLight(Vec3& pos, float& range) const {
  if(!isArpgDialogShot())
    return false;
  // a little above the camera and in front of it, towards the face
  const Vec3 view = arpgDlg.look-origin;
  const float l   = view.length();
  if(l<1.f)
    return false;
  pos   = origin + Vec3(0, 45.f, 0) + view*(std::min(40.f, l*0.3f)/l);
  range = l+180.f;
  return true;
  }

void Camera::updateArpgView(float dtF) {
  arpgV = ArpgView();
  const bool dlgView = (camMod==Dialog && isArpgEnabled() && camMarvinMod==M_Normal && arpgDlg.keepView);
  if(!isArpgActive() && !dlgView) {
    arpg.insideHold  = 0;
    arpg.nearCut     = 0;
    arpg.indoor      = false;
    arpg.indoorHold  = 0;
    arpg.indoorBlend = 0;
    arpg.seeThrough  = 0;
    arpg.losInit     = false;
    return;
    }

  const auto world = Gothic::inst().world();
  const auto pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr)
    return;

  const Vec3 feet   = pl->position();
  const Vec3 center = state.target;

  // indoor test: something above the head (ceiling, roof, cave, overhang),
  // or the floor belongs to an indoor sector of the world mesh
  bool roof = false;
  {
    auto&      physic = *world->physic();
    const Vec3 from   = center + Vec3(0,60,0);
    const Vec3 to     = from   + Vec3(0,2000,0);
    roof = physic.ray(from,to).hasCol;
    if(!roof) {
      auto down = physic.ray(center, feet-Vec3(0,100,0));
      roof = down.hasCol && down.sector!=nullptr && down.sector[0]!='\0';
      }
  }

  if(roof) {
    arpg.indoor     = true;
    arpg.indoorHold = 0.5f;
    }
  else if(dtF>0.f) {
    arpg.indoorHold -= dtF;
    if(arpg.indoorHold<=0.f)
      arpg.indoor = false;
    }

  arpgV.enabled    = true;
  arpgV.feet       = feet;
  arpgV.center     = center;
  arpgV.cutHeight  = arpg.indoor ? 250.f : 0.f;
  arpgV.cutRadius  = 3000.f;

  // see-through circle: fixed size in the world (~3.9 m around the hero's chest), so it does not change with zoom
  {
    const float dist = std::max(100.f, (center-origin).length());
    arpgV.fadeRadius = std::clamp(390.f/dist*std::fabs(proj.at(1,1)), 0.24f, 0.9f);
  }

  // ... and only while something actually hides the hero: rays camera -> head / chest / knees.
  // The part of a ray above the cut-away ceiling is skipped (invisible roofs and upper floors).
  {
    auto&       physic  = *world->physic();
    const float ceiling = arpgV.cutHeight>0.f ? feet.y+arpgV.cutHeight : std::numeric_limits<float>::max();
    bool        hidden  = false;
    for(float h:{170.f, 110.f, 50.f}) {
      const Vec3 to   = feet + Vec3(0,h,0);
      Vec3       from = origin;
      if(from.y>ceiling && to.y<ceiling) {
        const float t = (from.y-ceiling)/(from.y-to.y);
        from = from + (to-from)*t;
        }
      const auto r = physic.ray(from, to);
      if(r.hasCol && (r.v-to).quadLength() > 40.f*40.f) {
        hidden = true;
        break;
        }
      // and the other way (hero -> camera): the camera inside the rock or behind a wall facing the hero
      // (rays only see the outer side of surfaces)
      const auto b = physic.ray(to, from);
      if(b.hasCol && (b.v-to).quadLength() > 40.f*40.f) {
        hidden = true;
        break;
        }
      }
    const float dst = hidden ? 1.f : 0.f;
    if(dtF<0.f)
      arpg.seeThrough = dst; else
      arpg.seeThrough += std::clamp(dst-arpg.seeThrough, -2.5f*dtF, 5.f*dtF); // opens in 0.2 s, closes in 0.4 s
    arpgV.seeThrough = arpg.seeThrough;
  }

  // outdoors, the camera inside the rock (behind a ravine wall, in a mountain): the inner side of the world mesh is drawn
  // black (main.frag) and would fill the screen: between the camera and the hero it is not drawn.
  // Indoors the camera is always above the rock of the ceiling: the cut-away takes care of that.
  {
    auto& physic = *world->physic();
    bool  inside = false;
    if(arpgV.cutHeight<=0.f) {
      for(float h:{170.f, 90.f}) {
        const Vec3 to  = feet + Vec3(0,h,0);
        const auto r   = physic.rayBothSides(origin, to);
        // the first surface on the way is seen from its inner side (its normal points away from the camera)
        if(r.hasCol && Vec3::dotProduct(r.n, to-origin)>0.f) {
          inside = true;
          break;
          }
        }
      }
    if(inside)
      arpg.insideHold = 0.4f;
    else if(dtF>0.f)
      arpg.insideHold = std::max(0.f, arpg.insideHold-dtF);
    else
      arpg.insideHold = 0;
    const float dist = (center-origin).length();
    arpg.nearCut  = (arpg.insideHold>0.f) ? std::max(0.f, dist-60.f) : 0.f;
    arpgV.nearCut = arpg.nearCut;
  }

  // indoors: what is far below the hero's feet (cellars, the valley under a tower) fades to black
  {
    const float dst = arpg.indoor ? 1.f : 0.f;
    if(dtF<0.f)
      arpg.indoorBlend = dst; else
      arpg.indoorBlend += std::clamp(dst-arpg.indoorBlend, -2.f*dtF, 2.f*dtF);
    arpgV.indoor     = arpg.indoorBlend;
    arpgV.lowerStart = 350.f;
    arpgV.lowerEnd   = 900.f;
  }

  // line of sight (indoors): rays from the hero at knee and head height (+ one tilted up), a quarter of
  // the directions per frame; whatever the hero cannot see (behind cave walls) goes black in the fog pass
  {
    constexpr size_t N    = ArpgView::LosRays;
    const float      maxR = (arpgV.fowRadius>0.f ? arpgV.fowRadius+arpgV.fowFade : 3000.f);
    if(arpg.indoorBlend>0.f) {
      auto& physic = *world->physic();
      auto  measure = [&](size_t i) {
        const float a   = 2.f*float(M_PI)*float(i)/float(N);
        const Vec3  dir = {std::cos(a), 0.f, std::sin(a)};
        float best = 0;
        for(float h:{70.f, 160.f}) {
          const Vec3 from = feet + Vec3(0,h,0);
          const auto r    = physic.ray(from, from+dir*maxR);
          const float d   = r.hasCol ? std::hypot(r.v.x-from.x, r.v.z-from.z) : maxR;
          best = std::max(best, d);
          }
        {
          // tilted up by 20 degrees: ramps and stairs going up stay visible
          const Vec3 from = feet + Vec3(0,160.f,0);
          const Vec3 up   = Vec3(dir.x*0.94f, 0.342f, dir.z*0.94f);
          const auto r    = physic.ray(from, from+up*maxR);
          const float d   = r.hasCol ? std::hypot(r.v.x-from.x, r.v.z-from.z) : maxR*0.94f;
          best = std::max(best, d);
        }
        return std::min(best, maxR);
        };
      if(!arpg.losInit) {
        for(size_t i=0; i<N; ++i)
          arpg.losDst[i] = arpg.los[i] = measure(i);
        arpg.losInit = true;
        } else {
        for(size_t i=arpg.losPhase%4; i<N; i+=4)
          arpg.losDst[i] = measure(i);
        arpg.losPhase++;
        const float k = (dtF<0.f) ? 1.f : std::min(1.f, 10.f*dtF);
        for(size_t i=0; i<N; ++i)
          arpg.los[i] += (arpg.losDst[i]-arpg.los[i])*k;
        }
      } else {
      arpg.losInit = false;
      arpg.los.fill(maxR);
      }
    arpgV.los = arpg.los;
  }

  // fog of war: radius from the game's "sight range" option (Video menu, 20%..300%, 100% = 15 m)
  {
    int sight = 4;
    if(!Gothic::settingsGetS("PERFORMANCE","sightValue").empty())
      sight = Gothic::settingsGetI("PERFORMANCE","sightValue");
    sight = std::clamp(sight, 0, 14);
    const float scale = 0.2f + 0.2f*float(sight);
    arpgV.fowRadius   = 1500.f*scale;
    arpgV.fowFade     = std::max(300.f, arpgV.fowRadius*0.3f);
  }
  }
