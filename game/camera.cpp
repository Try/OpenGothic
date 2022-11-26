#include "camera.h"

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "game/definitions/cameradefinitions.h"
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

float Camera::maxDist        = 100;
float Camera::baseSpeeed     = 200;
float Camera::offsetAngleMul = 0.2f;

Camera::Camera() {
  (void)inertiaTarget; // OSX warning
  }

void Camera::reset() {
  reset(Gothic::inst().player());
  }

void Camera::reset(const Npc* pl) {
  const auto& def = cameraDef();
  dst.range  = userRange*(def.max_range-def.min_range)+def.min_range;
  dst.target = pl ? pl->cameraBone() : Vec3();

  dst.spin.x = def.best_elevation;
  dst.spin.y = pl ? pl->rotation() : 0;

  src.spin   = dst.spin;

  calcControlPoints(-1.f);
  }

void Camera::save(Serialize &s) {
  s.write(src.range, src.target, src.spin,
          dst.range, dst.target, dst.spin);
  s.write(cameraPos,origin,rotOffset);
  }

void Camera::load(Serialize &s, Npc* pl) {
  reset(pl);
  s.read(src.range, src.target, src.spin,
         dst.range, dst.target, dst.spin);
  s.read(cameraPos,origin,rotOffset);
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
  proj.perspective(65.f, float(w)/float(h), zNear(), zFar());
  vpWidth  = w;
  vpHeight = h;
  }

void Camera::rotateLeft() {
  implMove(KeyEvent::K_Q);
  }

void Camera::rotateRight() {
  implMove(KeyEvent::K_E);
  }

void Camera::moveForward() {
  implMove(KeyEvent::K_W);
  }

void Camera::moveBack() {
  implMove(KeyEvent::K_S);
  }

void Camera::moveLeft() {
  implMove(KeyEvent::K_A);
  }

void Camera::moveRight() {
  implMove(KeyEvent::K_D);
  }

void Camera::setMode(Camera::Mode m) {
  if(camMod==m)
    return;

  const bool reset = (m==Inventory || camMod==Inventory || camMod==Dialog || camMod==Dive);
  camMod = m;

  if(reset)
    resetDst();

  if(auto pl = Gothic::inst().player())
    dst.spin.y = pl->rotation();
  }

void Camera::setToogleEnable(bool e) {
  tgEnable = e;
  }

bool Camera::isToogleEnabled() const {
  return tgEnable;
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

void Camera::toogleDebug() {
  dbg = !dbg;
  }

void Camera::setSpin(const PointF &p) {
  dst.spin = Vec3(p.x,p.y,0);
  src.spin = dst.spin;
  }

void Camera::setDestSpin(const PointF& p) {
  dst.spin = Vec3(p.x,p.y,0);
  if(dst.spin.x<-90)
    dst.spin.x = -90;
  if(dst.spin.x>90)
    dst.spin.x = 90;
  }

void Camera::onRotateMouse(const PointF& dpos) {
  dst.spin.x += dpos.x;
  dst.spin.y += dpos.y;
  }

Matrix4x4 Camera::projective() const {
  auto ret = proj;
  if(auto w = Gothic::inst().world())
    w->globalFx()->morph(ret);
  return ret;
  }

Matrix4x4 Camera::viewShadow(const Vec3& lightDir, size_t layer) const {
  Vec3  ldir = lightDir;
  float eps  = 0.005f;

  if(std::abs(ldir.y)<eps) {
    float k = (1.f-eps*eps)/std::sqrt(ldir.x*ldir.x + ldir.z*ldir.z);
    ldir.y = (ldir.y<0) ? -eps : eps;
    ldir.x *= k;
    ldir.z *= k;
    }

  // if(layer==1)
  //   return viewShadow2(ldir,layer);

  Vec3 center = cameraPos;
  auto vp = viewProj();
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

  float rotation = (180+src.spin.y-rotOffset.y);

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

  // strectch shadowmap on light dir
  if(ldir.y!=0.f) {
    // stetch view to camera
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

float Camera::zNear() const {
  return 0.01f;
  }

float Camera::zFar() const {
  return 85.0f;
  }

const phoenix::c_camera &Camera::cameraDef() const {
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

void Camera::clampRotation(Tempest::Vec3& spin) {
  const auto& def = cameraDef();
  if(spin.x>def.max_elevation)
    spin.x = def.max_elevation;
  if(spin.x<def.min_elevation)
    ;//spin.x = def.minElevation;
  }

void Camera::implMove(Tempest::Event::KeyType key) {
  float dpos = 60.f;

  float k = -float(M_PI/180.0);
  float s = std::sin(dst.spin.x*k), c=std::cos(dst.spin.x*k);

  if(key==KeyEvent::K_A) {
    cameraPos.x+=dpos*c;
    cameraPos.z-=dpos*s;
    }
  if(key==KeyEvent::K_D) {
    cameraPos.x-=dpos*c;
    cameraPos.z+=dpos*s;
    }
  if(key==KeyEvent::K_W) {
    cameraPos.x-=dpos*s;
    cameraPos.z-=dpos*c;
    }
  if(key==KeyEvent::K_S){
    cameraPos.x+=dpos*s;
    cameraPos.z+=dpos*c;
    }
  if(auto world = Gothic::inst().world())
    cameraPos.y = world->physic()->landRay(cameraPos).v.y;
  }

void Camera::setPosition(const Tempest::Vec3& pos) {
  setDestPosition(pos);
  src.target = dst.target;
  cameraPos  = dst.target;
  }

void Camera::setDestPosition(const Tempest::Vec3& pos) {
  dst.target = pos;
  }

void Camera::setDialogDistance(float d) {
  dlgDist = d;
  }

void Camera::followPos(Vec3& pos, Vec3 dest, float dtF) {
  auto dp  = (dest-pos);
  auto len = dp.length();

  if(dtF<0.f) {
    pos = dest;
    return;
    }

  if(len<=0.01) {
    return;
    }

  float speed = baseSpeeed*dtF;
  float tr    = std::min(speed,len);
  if(len-tr>maxDist)
    tr = (len-maxDist); else
    tr = std::min(speed,len);

  float k = tr/len;
  pos += dp*k;
  }

void Camera::followCamera(Vec3& pos, Vec3 dest, float dtF) {
  const auto& def = cameraDef();
  if(!def.translate)
    return;
  if(dtF<0.f) {
    pos = dest;
    return;
    }
  auto  dp    = dest-pos;
  auto  len   = dp.length();
  if(len<=0.01) {
    return;
    }

  float speed = def.velo_trans*100.f*dtF;
  float tr    = std::min(speed,len);
  float k     = tr/len;
  pos += dp*k;
  }

void Camera::followAng(Vec3& spin, Vec3 dest, float dtF) {
  const auto& def = cameraDef();
  followAng(spin.x,dest.x,def.velo_rot,dtF);
  followAng(spin.y,dest.y,def.velo_rot,dtF);
  }

void Camera::followAng(float& ang, float dest, float speed, float dtF) {
  float da    = angleMod(dest-ang);
  float shift = da*std::min(1.f,2.f*speed*dtF);
  if(std::abs(da)<0.01f || dtF<0.f) {
    ang = dest;
    return;
    }

  static const float min=-45, max=45;
  if(da>max+1.f) {
    shift = (da-max);
    }
  if(da<min-1.f) {
    shift = (da-min);
    }
  ang += shift;
  }

void Camera::tick(uint64_t dt) {
  if(Gothic::inst().isPause())
    return;

  const float dtF = float(dt)/1000.f;

  {
  const auto& def = cameraDef();
  dst.range = def.min_range + (def.max_range-def.min_range)*userRange;
  const float zSpeed = 5.f;
  const float dz     = dst.range-src.range;
  src.range+=dz*std::min(1.f,2.f*zSpeed*dtF);
  }

  calcControlPoints(dtF);
  }

void Camera::calcControlPoints(float dtF) {
  const auto& def = cameraDef();
  auto  targetOffset = Vec3(def.target_offset_x,
                            def.target_offset_y,
                            def.target_offset_z);
  auto  rotOffsetDef = Vec3(def.rot_offset_x,
                            def.rot_offset_y,
                            def.rot_offset_z);
  auto  rotBest      = Vec3(0,def.best_azimuth,0);

  clampRotation(dst.spin);

  float range = src.range*100.f;
  if(camMod==Dialog) {
    // TODO: DialogCams.zen
    range        = dlgDist;
    src.spin     = dst.spin;
    src.target   = dst.target;
    cameraPos    = src.target;
    rotOffset    = Vec3();
    rotOffsetDef = Vec3();
    rotBest      = Vec3();
    //spin.y += def.bestAzimuth;
    }

  followAng(src.spin,  dst.spin+rotBest, dtF);
  followAng(rotOffset, rotOffsetDef,     dtF);

  Matrix4x4 rotOffsetMat;
  rotOffsetMat.identity();
  rotOffsetMat.rotateOY(180-src.spin.y);
  rotOffsetMat.rotateOX(src.spin.x);
  rotOffsetMat.project(targetOffset);

  Vec3 dir = {0,0,1};
  rotOffsetMat.project(dir);

  auto target = dst.target + targetOffset;

  followPos(src.target,target,dtF);
  followCamera(cameraPos,src.target,dtF);

  origin = cameraPos - dir*range;
  if(def.collision!=0) {
    range  = calcCameraColision(src.target,origin,src.spin,range);
    origin = cameraPos - dir*range;
    }

  auto baseOrigin = target - dir*range;
  if(camMod==Dialog)
    offsetAng = Vec3(); else
    offsetAng = calcOffsetAngles(origin,baseOrigin,dst.target);

  if(fpEnable) {
    origin    = dst.target;
    offsetAng = Vec3();

    Vec3 offset = {0,0,20};
    Matrix4x4 rotOffsetMat;
    rotOffsetMat.identity();
    rotOffsetMat.rotateOY(180-src.spin.y);
    rotOffsetMat.project(offset);
    origin += offset;
    }
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
  if(dst.length()>0.001f) {
    k = dot/dst.length();
    k = std::max(0.f,std::min(k/100.f,1.f));
    }

  auto  a0 = calcOffsetAngles(srcOrigin,target);
  auto  a1 = calcOffsetAngles(dstOrigin,target);
  auto  da = angleMod(a1-a0);
  return da*k*offsetAngleMul;
  }

float Camera::calcCameraColision(const Vec3& target, const Vec3& origin, const Vec3& rotSpin, float dist) const {
  if(camMod==Dialog)
    dist = dlgDist;

  auto world = Gothic::inst().world();
  if(world==nullptr)
    return dist;

  float minDist = 20;
  float padding = 20;

  auto& physic = *world->physic();
  Matrix4x4 vinv=projective();
  vinv.mul(mkView(origin,rotSpin));
  vinv.inverse();

  raysCasted = 0;
  float distMd = dist;
  auto  tr     = origin - target;
  static int n = 1, nn=1;
  for(int i=-n;i<=n;++i)
    for(int r=-n;r<=n;++r) {
      raysCasted++;
      float u = float(i)/float(nn),v = float(r)/float(nn);
      Tempest::Vec3 r0 = target;
      Tempest::Vec3 r1 = {u,v,0};

      vinv.project(r1.x,r1.y,r1.z);

      auto rc = physic.ray(r0, r1);
      auto d  = rc.v;
      d -=r0;
      r1-=r0;

      float dist0 = r1.length();
      float dist1 = Vec3::dotProduct(d,tr)/dist;
      if(!rc.hasCol)
        continue;

      dist1 = std::max<float>(0,dist1-padding);
      float md = dist-std::max(0.f,dist0-dist1);
      if(md<distMd)
        distMd=md;
      }
  return std::max(minDist,distMd);
  }

Matrix4x4 Camera::mkView(const Vec3& pos, const Vec3& spin) const {
  static float scale = 0.0009f;

  Matrix4x4 view;
  view.identity();
  view.scale(-1,-1,-1);
  view.scale(scale);

  view.mul(mkRotation(spin));
  view.translate(-pos);

  return view;
  }

Matrix4x4 Camera::mkRotation(const Vec3& spin) const {
  Matrix4x4 view;
  view.identity();
  view.rotateOX(spin.x-rotOffset.x);
  view.rotateOY(spin.y-rotOffset.y);
  view.rotateOZ(spin.z-rotOffset.z);
  return view;
  }

void Camera::resetDst() {
  const auto& def = cameraDef();
  dst.spin.x = def.best_elevation;
  dst.range  = def.best_range;
  }

void Camera::debugDraw(DbgPainter& p) {
  if(!dbg)
    return;

  p.setPen(Color(0,1,0));
  p.drawLine(dst.target, src.target);

  if(auto pl = Gothic::inst().player()) {
    float a  = pl->rotationRad();
    float c  = std::cos(a), s = std::sin(a);
    auto  ln = Vec3(c,0,s)*25.f;
    p.drawLine(src.target-ln, src.target+ln);
    }

  auto& fnt = Resources::font();
  int   y   = 300+fnt.pixelSize();

  string_frm buf("RaysCasted: ",raysCasted);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("PlayerPos : ",dst.target.x, ' ', dst.target.y, ' ', dst.target.z);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("Range To Player : ", (dst.target-origin).length());
  p.drawText(8,y,buf); y += fnt.pixelSize();

  buf = string_frm("Azimuth : ", angleMod(dst.spin.y-src.spin.y));
  p.drawText(8,y,buf); y += fnt.pixelSize();
  buf = string_frm("Elevation : ", rotOffset.x-src.spin.x);
  p.drawText(8,y,buf); y += fnt.pixelSize();
  }

PointF Camera::spin() const {
  return PointF(src.spin.x,src.spin.y);
  }

PointF Camera::destSpin() const {
  return PointF(dst.spin.x,dst.spin.y);
  }

Matrix4x4 Camera::viewProj() const {
  Matrix4x4 ret=projective();
  ret.mul(view());
  return ret;
  }

Matrix4x4 Camera::view() const {
  auto spin = src.spin+offsetAng;
  return mkView(origin,spin);
  }
