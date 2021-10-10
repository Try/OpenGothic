#include "camera.h"

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"
#include "game/playercontrol.h"
#include "game/definitions/cameradefinitions.h"
#include "game/serialize.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"
#include "gothic.h"

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

// TODO: System/Camera/CamInst.d
Camera::Camera() {
  }

void Camera::reset(World& world) {
  auto pl = world.player();
  if(pl==nullptr)
    return;
  implReset(*pl);
  }

void Camera::implReset(const Npc &npc) {
  const auto& def = cameraDef();
  dst.range = def.bestRange;
  cameraPos = dst.target;
  calcControlPoints(npc,true,0);
  src = dst;
  }

void Camera::save(Serialize &s) {
  Tempest::Vec3 unused;
  s.write(src.target, src.spin, unused, src.range,
          dst.target, dst.spin, unused, dst.range,
          hasPos);
  }

void Camera::load(Serialize &s, Npc *pl) {
  if(pl)
    implReset(*pl);
  if(s.version()<24)
    return;
  Tempest::Vec3 unused;
  s.read(src.target, src.spin, unused, src.range,
         dst.target, dst.spin, unused, dst.range,
         hasPos);
  }

void Camera::changeZoom(int delta) {
  if(delta>0)
    dst.range-=0.1f; else
    dst.range+=0.1f;
  clampRange(dst.range);
  }

void Camera::setViewport(uint32_t w, uint32_t h) {
  proj.perspective(65.f, float(w)/float(h), 0.01f, 85.0f);
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

  camMod = m;

  const auto& def = cameraDef();
  dst.spin.y = def.bestAzimuth;
  if(auto pl = Gothic::inst().player())
    dst.spin.y+=pl->rotation();
  //dst.spin.x = def.bestElevation;
  dst.range  = def.bestRange;
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
  lbEnable = lb;
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
  //Vec3 ldir = Vec3::normalize({1,1,0});
  Vec3 ldir   = lightDir;
  Vec3 center = cameraPos;
  auto vp = viewProj();
  vp.project(center);

  vp.inverse();
  Vec3 l = {-1,center.y,center.z}, r = {1,center.y,center.z};
  vp.project(l);
  vp.project(r);

  float smWidth    = 0;
  float smWidthInv = 0;
  float zScale     = 1.f/5120;

  switch(layer) {
    case 0:
      smWidth    = (r-l).length();
      smWidth    = std::max(smWidth,1024.f); // ~4 pixels per santimeter
      break;
    case 1:
      smWidth    = 5120;
      zScale    *= 0.2f;
      break;
    };

  smWidthInv = 1.f/smWidth;

  Matrix4x4 view;
  view.identity();
  view.rotate(-90, 1, 0, 0);     // +Y -> -Z
  view.rotate(180+src.spin.y, 0, 1, 0);
  view.scale(smWidthInv, zScale, smWidthInv);
  view.translate(cameraPos);
  view.scale(-1,-1,-1);

  if(ldir.y!=0.f) {
    float lx = ldir.x/ldir.y;
    float lz = ldir.z/ldir.y;

    const float ang = -(180+src.spin.y)*float(M_PI)/180.f;
    const float c   = std::cos(ang), s = std::sin(ang);

    float dx = lx*c-lz*s;
    float dz = lx*s+lz*c;

    view.set(1,0, dx*smWidthInv);
    view.set(1,1, dz*smWidthInv);
    }

  if(layer>0) {
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

  Tempest::Matrix4x4 proj;
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
  return proj;
  }

const Daedalus::GEngineClasses::CCamSys &Camera::cameraDef() const {
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

void Camera::clampRange(float &zoom) {
  const auto& def = cameraDef();
  if(zoom>def.maxRange)
    zoom = def.maxRange;
  if(zoom<def.minRange)
    zoom = def.minRange;
  }

void Camera::clampRotation(Tempest::Vec3& spin) {
  const auto& def = cameraDef();
  if(spin.x>def.maxElevation)
    spin.x = def.maxElevation;
  if(spin.x<def.minElevation)
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

void Camera::setPosition(float x, float y, float z) {
  dst.target = {x,y,z};
  cameraPos  = dst.target;
  }

void Camera::setDestPosition(const Tempest::Vec3& pos) {
  dst.target = pos;
  }

void Camera::setDialogDistance(float d) {
  dlgDist = d;
  }

void Camera::followPos(Vec3& pos, Vec3 dest, float dtF) {
  if(camMod==Dialog || camMod==Mobsi) {
    pos = dest;
    return;
    }

  auto dp  = (dest-pos);
  auto len = dp.length();

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
  if(camMod==Dialog || camMod==Mobsi) {
    pos = dest;
    return;
    }

  auto  dp    = dest-pos;
  auto  len   = dp.length();
  if(len<=0.01) {
    return;
    }

  float speed = def.veloTrans*100.f*dtF;
  float tr    = std::min(speed,len);
  float k     = tr/len;
  pos += dp*k;
  }

void Camera::followAng(Vec3& spin, Vec3 dest, float dtF) {
  const auto& def = cameraDef();
  followAng(spin.x,dest.x,def.veloRot,dtF);
  followAng(spin.y,dest.y,def.veloRot,dtF);
  }

void Camera::followAng(float& ang, float dest, float speed, float dtF) {
  float da    = angleMod(dest-ang);
  float shift = da*std::min(1.f,2.f*speed*dtF);
  if(std::abs(da)<0.01f) {
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

void Camera::tick(const Npc& npc, uint64_t dt, bool includeRot) {
  if(!hasPos) {
    dst    = src;
    hasPos = true;
    }

  if(Gothic::inst().isPause())
    return;

  const float dtF = float(dt)/1000.f;
  clampRange(dst.range);

  {
  const float zSpeed = 5.f;
  const float dz     = dst.range-src.range;
  src.range+=dz*std::min(1.f,2.f*zSpeed*dtF);
  }

  calcControlPoints(npc,dtF,includeRot);
  }

void Camera::calcControlPoints(const Npc& npc, float dtF, bool includeRot) {
  const auto& def = cameraDef();
  auto  targetOffset = Vec3(def.targetOffsetX,
                            def.targetOffsetY,
                            def.targetOffsetZ);
  auto  rotOffsetDef = Vec3(def.rotOffsetX,
                            def.rotOffsetY,
                            def.rotOffsetZ);

  auto  pos          = npc.cameraBone();
  float range        = src.range*100.f;

  followAng(src.spin,  dst.spin,     dtF);
  followAng(rotOffset, rotOffsetDef, dtF);
  if(includeRot)
    clampRotation(rotOffset);

  Matrix4x4 rotOffsetMat;
  rotOffsetMat.identity();
  rotOffsetMat.rotateOY(180-src.spin.y);
  rotOffsetMat.rotateOX(src.spin.x);
  rotOffsetMat.project(targetOffset);

  rotOffsetMat.rotateOX(rotOffset.x);
  rotOffsetMat.rotateOY(rotOffset.y);
  rotOffsetMat.rotateOZ(rotOffset.z);

  Vec3 dir = {0,0,1};
  rotOffsetMat.project(dir);

  dst.target  = pos + targetOffset;

  followPos(src.target,dst.target,dtF);
  followCamera(cameraPos,src.target,dtF);

  auto baseOrigin = dst.target - dir*range;
  origin          = cameraPos  - dir*range;

  if(def.collision) {
    range      = calcCameraColision(src.target,origin,src.spin,range);
    baseOrigin = dst.target - dir*range*100.f;
    origin     = cameraPos  - dir*range*100.f;
    }

  offsetAng = calcOffsetAngles(origin,baseOrigin,dst.target);
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
  return std::max(minDist,distMd)/100.f;
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

  if(auto pl = Gothic::inst().player()) {
    float a  = pl->rotationRad();
    float c  = std::cos(a), s = std::sin(a);
    auto  ln = Vec3(c,0,s)*25.f;
    p.drawLine(src.target-ln, src.target+ln);
    }

  auto& fnt = Resources::font();
  int   y   = 300+fnt.pixelSize();
  char  buf[256] = {};

  std::snprintf(buf,sizeof(buf),"RaysCasted : %d", raysCasted);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"PlayerPos : %f %f %f", dst.target.x, dst.target.y, dst.target.z);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"Range To Player : %f", (dst.target-origin).length());
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"Azimuth : %f", angleMod(dst.spin.y-src.spin.y));
  p.drawText(8,y,buf); y += fnt.pixelSize();
  std::snprintf(buf,sizeof(buf),"Elevation : %f", src.spin.x-rotOffset.x);
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
  //return mkView(origin,offsetAng);
  return mkView(origin,src.spin+offsetAng);
  }
