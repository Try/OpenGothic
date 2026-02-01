#include "camera.h"

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

static Vec3 angleMod(Vec3 a) {
  a.x = angleMod(a.x);
  a.y = angleMod(a.y);
  a.z = angleMod(a.z);
  return a;
  }

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

float       Camera::maxDist          = 150;
float       Camera::baseSpeeed       = 200;
float       Camera::offsetAngleMul   = 0.1f;
const float Camera::minLength        = 0.0001f;

Camera::Camera() {
  }

void Camera::reset() {
  reset(Gothic::inst().player());
  }

void Camera::reset(const Npc* pl) {
  const auto& def = cameraDef();
  if(userRange<=0) {
    userRange = (def.best_range - def.min_range)/(def.max_range - def.min_range);
    }
  dst.range    = userRange*(def.max_range-def.min_range)+def.min_range;
  dst.target   = pl ? pl->cameraBone() : Vec3();

  dst.spin.x   = 0;
  dst.spin.y   = pl ? pl->rotation() : 0;
  dst.spin    += Vec3(def.best_elevation,
                      def.best_azimuth,
                      def.best_rot_z);

  src.spin     = dst.spin;

  tickThirdPerson(-1.f);
  }

void Camera::save(Serialize &s) {
  s.write(src.range, src.target, src.spin,
          dst.range, dst.target, dst.spin);
  s.write(origin,angles);
  s.write(rotOffset);
  }

void Camera::load(Serialize &s, Npc* pl) {
  reset(pl);
  if(s.version()<54)
    return;
  s.read(src.range, src.target, src.spin,
         dst.range, dst.target, dst.spin);
  s.read(origin,angles);
  s.read(rotOffset);
  }

void Camera::changeZoom(int delta) {
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

void Camera::setMode(Camera::Mode m) {
  if(camMod==m)
    return;

  const bool reset = (m==Inventory || camMod==Inventory || camMod==Dialog || camMod==Dive || m==Fall || camMod==Fall);
  camMod = m;

  if(camMarvinMod==M_Freeze)
    return;

  if(reset) {
    resetDst();
    // auto ang = calcOffsetAngles(src.target,dst.target);
    // dst.spin.x = ang.x;
    }

  if(auto pl = Gothic::inst().player()) {
    dst.spin.x = 0;
    dst.spin.y = pl->rotation();
    }

  const auto& def = cameraDef();
  auto  rotBest   = Vec3(def.best_elevation,
                         def.best_azimuth,
                         def.best_rot_z);
  dst.spin += rotBest;
  }

void Camera::setMarvinMode(Camera::MarvinMode nextMod) {
  if(camMarvinMod==nextMod)
    return;

  /*
  if(camMarvinMod==M_Pinned) {
    src.spin     = dst.spin;
    float range  = src.range*100.f;
    Vec3  dir    = {0,0,1};
    Matrix4x4 rotOffsetMat;
    rotOffsetMat.identity();
    rotOffsetMat.rotateOY(180-src.spin.y);
    rotOffsetMat.rotateOX(src.spin.x);
    rotOffsetMat.project(dir);
    dst.target  = origin +dir*range;
    src.target  = dst.target;
    cameraPos   = src.target;
    rotOffset.y = 0;
    }
  */

  if(nextMod==M_Pinned) {
    const auto pl = Gothic::inst().player();

    auto      offset = origin - dst.target;
    Matrix4x4 rotMat = pl!=nullptr ? pl->cameraMatrix(false) : Matrix4x4::mkIdentity();
    rotMat.inverse();
    rotMat.project(offset);

    pin.origin = offset;//origin - dst.target;
    pin.spin   = angles;
    }
  else if(nextMod==M_Free) {
    dst.spin   = angles;
    dst.target = origin;
    src = dst;
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
  resetDst();
  }

void Camera::toggleDebug() {
  dbg = !dbg;
  }

void Camera::setSpin(const PointF &p) {
  dst.spin = Vec3(p.x,p.y,0);
  src.spin = dst.spin;
  }

void Camera::setDestSpin(const PointF& p) {
  if(camMarvinMod==M_Free || camMarvinMod==M_Freeze)
    return;
  dst.spin = Vec3(p.x,p.y,0);
  if(dst.spin.x<-90)
    dst.spin.x = -90;
  if(dst.spin.x>90)
    dst.spin.x = 90;
  }

void Camera::setTarget(const Tempest::Vec3& pos) {
  dst.target = pos;
  src.target = pos;
  }

void Camera::setDestTarget(const Tempest::Vec3& pos) {
  //if(camMarvinMod!=M_Free && (camMarvinMod!=M_Freeze || camMod==Dialog))
  dst.target = pos;
  }

void Camera::onRotateMouse(const PointF& dpos) {
  if(camMarvinMod==M_Freeze)
    return;
  dst.spin.x += dpos.x;
  dst.spin.y += dpos.y;
  }

Matrix4x4 Camera::projective() const {
  auto ret = proj;
  if(auto w = Gothic::inst().world())
    w->globalFx()->morph(ret);
  return ret;
  }

Matrix4x4 Camera::viewShadowVsm(const Tempest::Vec3& ldir) const {
  return mkViewShadowVsm(src.target,ldir);
  }

Matrix4x4 Camera::viewShadowVsmLwc(const Tempest::Vec3& ldir) const {
  return mkViewShadowVsm(src.target-origin,ldir);
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
  float rotation = (180+angles.y);
  // if(layer==0)
  //   return viewShadowVsm(cameraPos,rotation,vp,lightDir);
  return mkViewShadow(src.target,rotation,vp,lightDir,layer);
  }

Matrix4x4 Camera::viewShadowLwc(const Tempest::Vec3& lightDir, size_t layer) const {
  auto  vp       = viewProjLwc();
  float rotation = (180+angles.y);
  // if(layer==0)
  //   return viewShadowVsm(cameraPos-origin,rotation,vp,lightDir);
  return mkViewShadow(src.target-origin,rotation,vp,lightDir,layer);
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
  float s         = std::sin(angles.y*k), c=std::cos(angles.y*k);
  float sx        = std::sin(angles.x*k);
  float cx        = std::cos(angles.x*k);

  if(key==KeyEvent::K_A) {
    dst.target.x += dpos*c;
    dst.target.z += dpos*s;
    }
  if(key==KeyEvent::K_D) {
    dst.target.x -= dpos*c;
    dst.target.z -= dpos*s;
    }
  if(key==KeyEvent::K_W) {
    dst.target.x += dpos*s*cx;
    dst.target.z -= dpos*c*cx;
    dst.target.y -= dpos*sx;
    }
  if(key==KeyEvent::K_S) {
    dst.target.x -= dpos*s*cx;
    dst.target.z += dpos*c*cx;
    dst.target.y += dpos*sx;
    }
  if(key==KeyEvent::K_Q)
    dst.spin.y += dRot;
  if(key==KeyEvent::K_E)
    dst.spin.y -= dRot;
  }

void Camera::setPosition(const Tempest::Vec3& pos) {
  origin = pos;
  }

void Camera::setDialogDistance(float d) {
  dlgDist = d;
  }

void Camera::followPos(Vec3& pos, Vec3 dest, float dtF) {
  const auto& def = cameraDef();

  auto dp  = (dest-pos);
  auto len = dp.length();

  if(dtF<=0.f) {
    pos = dest;
    return;
    }

  if(len<=minLength) {
    return;
    }

  static float mul  = 2.1f;
  static float mul2 = 10.f;
  targetVelo = targetVelo + (len-targetVelo)*std::min(1.f,dtF*mul2);

  if(inertiaTarget) {
    veloTrans = std::min(def.velo_trans*100, targetVelo*mul);
    } else {
    veloTrans = def.velo_trans;
    }

  float tr = std::min(veloTrans*dtF,len);
  float k  = tr/len;
  pos += dp*k;

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
  }

void Camera::followCamera(Vec3& pos, Vec3 dest, float dtF) {
  if(dtF<0.f) {
    pos = dest;
    return;
    }

  const auto& def = cameraDef();
  if(!def.translate)
    return;
  pos = pos + (dest-pos)*dtF;
  }

void Camera::followSpin(Tempest::Vec3& spin, Tempest::Vec3 dest, float dtF) {
  if(dtF<0.f) {
    spin = dest;
    return;
    }

#if 1
  static float gvelo = 10.f;
  const zenkit::Quat sx = fromAngles(spin);
  const zenkit::Quat dx = fromAngles(dest);

  const zenkit::Quat s  = slerp(sx, dx, dtF*gvelo);
  spin = toAngles(s);
#else
  const zenkit::Quat d = fromAngles(dst.spin);
  src.spin = toAngles(d);
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

  const float dtF = float(dt)/1000.f;

  {
    const auto& def = cameraDef();
    dst.range = def.min_range + (def.max_range-def.min_range)*userRange;
    const float zSpeed = 5.f;
    const float dz     = dst.range-src.range;
    src.range+=dz*std::min(1.f,2.f*zSpeed*dtF);
  }

  auto prev = origin;

  switch (camMarvinMod) {
    case M_Normal: {
      if(isCutscene()) {
        src.target = dst.target;
        src.spin   = dst.spin;
        origin     = src.target;
        angles     = src.spin;
        }
      else if(fpEnable && camMod!=Dialog) {
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
      followSpin(src.spin, dst.spin, dtF);
      src.target = dst.target;
      origin     = src.target;
      angles     = src.spin;
      break;
      }
    case M_Pinned: {
      const auto pl     = Gothic::inst().player();
      const auto rotMat = pl!=nullptr ? pl->cameraMatrix(false) : Matrix4x4::mkIdentity();

      auto offset = pin.origin;
      rotMat.project(offset);
      origin     = dst.target + offset;
      angles     = pin.spin;
      break;
      }
    }

  auto world = Gothic::inst().world();
  if(world!=nullptr) {
    auto  pl     = isFree() ? nullptr : world->player();
    auto& physic = *world->physic();

    if(pl!=nullptr && !pl->isInWater()) {
      inWater = physic.cameraRay(src.target, origin).waterCol % 2;
      } else {
      // NOTE: find a way to avoid persistent tracking
      inWater = inWater ^ (physic.cameraRay(prev, origin).waterCol % 2);
      }
    }
  }

void Camera::tickFirstPerson(float /*dtF*/) {
  const auto pl     = Gothic::inst().player();
  const auto rotMat = pl!=nullptr ? pl->cameraMatrix(true) : Matrix4x4::mkIdentity();

  //dst.spin.y = pl!=nullptr ? pl->rotation() : 0;
  //dst.spin.z = 0;

  dst.spin   = clampRotation(dst.spin);
  src.spin   = dst.spin;
  src.target = dst.target;

  Vec3 offset = {0,0,20};
  rotMat.project(offset);
  origin = offset;
  angles = dst.spin;
  }

void Camera::tickThirdPerson(float dtF) {
  //const auto  pl  = Gothic::inst().player();
  const auto& def = cameraDef();

  auto mkRotMatrix = [](Vec3 spin){
    auto rotOffsetMat = Matrix4x4::mkIdentity();
    rotOffsetMat.rotateOY(180-spin.y);
    rotOffsetMat.rotateOX(spin.x);
    rotOffsetMat.rotateOZ(spin.z);
    return rotOffsetMat;
    };

  auto  targetOffset = Vec3(def.target_offset_x,
                            def.target_offset_y,
                            def.target_offset_z);
  auto  rotOffsetDef = Vec3(def.rot_offset_x,
                            def.rot_offset_y,
                            def.rot_offset_z);
  auto  range        = (camMod==Dialog) ? dlgDist : src.range*100.f;

  if(camMod==Dialog) {
    // TODO: DialogCams.zen
    src.target = dst.target;
    src.spin   = dst.spin;
    rotOffset  = Vec3(0);
    }

  if(camMod!=Dialog) {
    dst.spin = clampRotation(dst.spin);

    followAng (rotOffset, rotOffsetDef, dtF);
    //followSpin(src.spin,  dst.spin,     dtF);
    src.spin = dst.spin;
    }

  const auto rotOffsetMat = mkRotMatrix(src.spin);
  if(camMod!=Dialog) {
    rotOffsetMat.project(targetOffset);
    // vanilla clamps target-offset according to collision
    if(def.collision!=0) {
      targetOffset = calcCameraColision(dst.target, targetOffset);
      }
    // and has leah-like follow for target+offset
    followPos(src.target, dst.target+targetOffset, dtF);
    }

  auto dir = Vec3{0,0,-1};
  rotOffsetMat.project(dir);

  if(true && def.collision!=0) {
    auto rotation = calcLookAtAngles(src.target + dir*range, src.target, rotOffset);
    // testd in marvin: collision is calculated from offseted 'target', not from npc
    range = calcCameraColision(src.target,dir,rotation,range);
    // NOTE: with range < 80, camera gradually moves up in vanilla
    if(range<80.f) {
      range      = 150;
      rotation.x = 90;
      const auto rotOffsetMat = mkRotMatrix(rotation);//!
      dir = Vec3{0,0,-1};
      rotOffsetMat.project(dir);
      }
    }

  auto followTranslation = [](Vec3 a, Vec3 b, float dtF, float velo) {
    if(dtF<0)
      return b;
    static float k = 10.f;
    return a + (b-a)*std::min(1.f, k*dtF);
    };

  origin = followTranslation(origin, src.target + dir*range, (camMod!=Dialog ? dtF : -1.f), def.velo_trans);
  angles = calcLookAtAngles(origin, src.target, rotOffset);

  static bool dbg = false;
  if(dbg) {
    origin = dst.target + targetOffset + dir*range;
    angles = calcLookAtAngles(origin, dst.target, rotOffset);
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

Vec3 Camera::calcLookAtAngles(const Tempest::Vec3& origin, const Tempest::Vec3& target, const Vec3& rotOffset) const {
  auto  sXZ = (origin - target);

  float y0  = std::atan2(sXZ.x,sXZ.z)*180.f/float(M_PI);
  float x0  = std::atan2(sXZ.y,Vec2(sXZ.x,sXZ.z).length())*180.f/float(M_PI);

  return Vec3(x0,-y0,0) - rotOffset;
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
  float       maxElev = +90;
  float       minElev = -60;
  float       maxAzim = +180;
  float       minAzim = -180;

  const auto pl = Gothic::inst().player();
  if(pl==nullptr)
    return spin;

  Vec3 dspin = Vec3{0, pl->rotation(), 0};
  spin -= dspin;

  spin.x = std::clamp(spin.x, minElev, maxElev);
  spin.y = std::clamp(spin.y, minAzim, maxAzim);
  return spin + dspin;
  }

Vec3 Camera::calcOffsetAngles(const Vec3& origin, const Vec3& target) const {
  auto  sXZ = origin-target;
  float y0  = std::atan2(sXZ.x,sXZ.z)*180.f/float(M_PI);
  float x0  = std::atan2(sXZ.y,Vec2(sXZ.x,sXZ.z).length())*180.f/float(M_PI);

  return Vec3(x0,-y0,0);
  }

Vec3 Camera::calcOffsetAngles(Vec3 srcOrigin, Vec3 dstOrigin, Vec3 target) const {
  auto  src = srcOrigin-target; src.y = 0;
  auto  dst = dstOrigin-target; dst.y = 0;

  auto  dot = Vec3::dotProduct(src,dst);
  float k   = 0;
  if(dst.length()>minLength) {
    k = dot/dst.length();
    k = std::max(0.f,std::min(k/100.f,1.f));
    }

  auto  a0 = calcOffsetAngles(srcOrigin,target);
  auto  a1 = calcOffsetAngles(dstOrigin,target);
  auto  da = angleMod(a1-a0);
  return da*k*offsetAngleMul;
  }

void Camera::resetDst() {
  if(isMarvin())
    return;
  const auto& def = cameraDef();
  // dst.spin.x = def.best_elevation;
  dst.range  = def.best_range;
  dst.spin   = Vec3(0);
  }

Matrix4x4 Camera::mkView(const Vec3& pos, const Vec3& spin) const {
  Matrix4x4 view;
  view.identity();
  view.scale(-1,-1,-1);

  // view.translate(0,0,-zNear());
  view.mul(mkRotation(spin));
  view.translate(-pos);

  return view;
  }

Matrix4x4 Camera::mkRotation(const Vec3& spin) const {
  Matrix4x4 view;
  view.identity();
  view.rotateOX(spin.x);
  view.rotateOY(spin.y);
  view.rotateOZ(spin.z);
  return view;
  }

void Camera::debugDraw(DbgPainter& p) {
  if(!dbg)
    return;

  p.setPen(Color(0,1,0));
  p.drawLine(dst.target, src.target);
  p.drawLine(src.target, origin);

  if(auto pl = Gothic::inst().player()) {
    float a  = pl->rotationRad();
    float c  = std::cos(a), s = std::sin(a);
    auto  ln = Vec3(c,0,s)*25.f;
    p.drawLine(src.target-ln, src.target+ln);
    }

  auto& fnt = Resources::font(1.0);
  int   y   = 300+fnt.pixelSize();

  string_frm buf("RaysCasted: ",raysCasted);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("PlayerPos : ",dst.target.x, ' ', dst.target.y, ' ', dst.target.z);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("targetVelo : ",targetVelo);
  p.drawText(8,y,buf); y += fnt.pixelSize()*4;

  buf = string_frm("Range To Player : ", (src.target-origin).length());
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("Azimuth : ", angleMod(dst.spin.y-angles.y));
  p.drawText(8,y,buf); y += fnt.pixelSize();
  buf = string_frm("Elevation : ", rotOffset.x+angles.x);
  p.drawText(8,y,buf); y += fnt.pixelSize();
  }

PointF Camera::spin() const {
  return PointF(src.spin.x,src.spin.y);
  }

PointF Camera::destSpin() const {
  return PointF(dst.spin.x,dst.spin.y);
  }

Vec3 Camera::destTarget() const {
  return dst.target;
  }

Matrix4x4 Camera::viewProj() const {
  Matrix4x4 ret=projective();
  ret.mul(view());
  return ret;
  }

Matrix4x4 Camera::view() const {
  return mkView(origin, angles);
  }

Matrix4x4 Camera::viewLwc() const {
  return mkView(Vec3(0), angles);
  }

Matrix4x4 Camera::viewProjLwc() const {
  Matrix4x4 ret=projective();
  ret.mul(viewLwc());
  return ret;
  }
