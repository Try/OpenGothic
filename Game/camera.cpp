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

static float angleMod(float a){
  a = std::fmod(a,360.f);
  if(a<-180.f)
    a+=360.f;
  if(a>180.f)
    a-=360.f;
  return a;
  }

// TODO: System/Camera/CamInst.d
Camera::Camera(Gothic &gothic) : gothic(gothic) {
  }

void Camera::reset(World& world) {
  auto pl = world.player();
  if(pl==nullptr)
    return;
  implReset(*pl);
  }

void Camera::implReset(const Npc &npc) {
  auto& def  = cameraDef();

  state.range  = dest.range;
  dest.pos     = npc.cameraBone();
  dest.spin.y  = def.bestAzimuth;
  dest.spin.y += npc.rotation();
  dest.spin.x  = def.bestElevation;
  state.spin   = dest.spin;
  state.spin2  = applyModRotation(dest.spin);

  state.pos    = applyModPosition(dest.pos);

  state.range  = def.bestRange;
  dest.range   = state.range;
  }

void Camera::save(Serialize &s) {
  s.write(state.pos, state.spin, state.spin2, state.range,
          dest.pos,  dest.spin,  dest.spin2,  dest.range,
          hasPos);
  }

void Camera::load(Serialize &s, Npc *pl) {
  if(pl)
    implReset(*pl);
  if(s.version()<24)
    return;
  s.read(state.pos, state.spin, state.spin2, state.range,
         dest.pos,  dest.spin,  dest.spin2,  dest.range,
         hasPos);
  }

void Camera::changeZoom(int delta) {
  if(delta>0)
    dest.range-=0.1f; else
    dest.range+=0.1f;
  clampRange(dest.range);
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
  dest.spin.y = def.bestAzimuth;
  if(auto pl=gothic.player())
    dest.spin.y+=pl->rotation();
  dest.spin.x = def.bestElevation;
  dest.range  = def.bestRange;
  }

void Camera::setToogleEnable(bool e) {
  tgEnable = e;
  }

bool Camera::isToogleEnabled() const {
  return tgEnable;
  }

void Camera::toogleDebug() {
  dbg = !dbg;
  }

void Camera::setSpin(const PointF &p) {
  dest.spin   = Vec3(p.x,p.y,0);
  state.spin  = dest.spin;
  state.spin2 = applyModRotation(dest.spin);
  }

void Camera::setDestSpin(const PointF& p) {
  dest.spin = Vec3(p.x,p.y,0);
  if(dest.spin.x<-90)
    dest.spin.x = -90;
  if(dest.spin.x>90)
    dest.spin.x = 90;
  }

void Camera::onRotateMouse(const PointF& dpos) {
  dest.spin.x += dpos.x;
  dest.spin.y += dpos.y;
  }

Matrix4x4 Camera::projective() const {
  return proj;
  }

Matrix4x4 Camera::viewShadow(const Vec3& ldir, size_t layer) const {
  //static float scale = 0.0008f;
  const  float c = std::cos(state.spin.y*float(M_PI)/180.f), s = std::sin(state.spin.y*float(M_PI)/180.f);

  Vec3 center = state.pos;
  auto vp = viewProj();
  vp.project(center);

  vp.inverse();
  Vec3 l = {-1,center.y,center.z}, r = {1,center.y,center.z};
  vp.project(l);
  vp.project(r);

  float smWidth    = (r-l).manhattanLength();
  float smWidthInv = 2.f/smWidth;
  float zScale     = 1.0;
  Matrix4x4 view;

  switch(layer) {
    case 0:
      smWidthInv *= 0.5;
      zScale      = 0.05f;
      break;
    case 1:
      smWidthInv = 0.001558001502f;
      smWidthInv *= 0.125;
      zScale      = 0.005f;
      break;
    };

  view.identity();
  view.translate(0.f,0.8f,0.5f);
  view.rotate(-90, 1, 0, 0);     // +Y -> -Z
  view.rotate(180+state.spin.y, 0, 1, 0);
  view.scale(smWidthInv, smWidthInv*zScale, smWidthInv);
  view.translate(state.pos);
  view.scale(-1,-1,-1);

/*
  auto inv = view;
  inv.inverse();
  float cx=0,cy=0,cz=0;
  inv.project(cx,cy,cz);
  cy=state.pos.y;

  float lx  = view.at(1,0);
  float ly  = view.at(1,1);
  float lz  = view.at(1,2);
  float k   = ldir.y!=0.f ? lz/ldir.y : 0.f;

  lx = -ldir.x*k;
  ly =  ldir.z*k;

  lz =  ldir.y*k;
  lz =  lz * (zScale/smWidthInv);

  float dx = lx*c-ly*s;
  float dy = lx*s+ly*c;

  view.set(1,0, dx);
  view.set(1,1, dy);
  view.set(1,2, lz);

  view.project(cx,cy,cz);
  view.set(3,0, view.at(3,0)-cx);
  view.set(3,1, view.at(3,1)-cy);
*/
  return view;
  }

Vec3 Camera::applyModPosition(const Vec3& pos) {
  const auto& def = cameraDef();

  Vec3 targetOffset = Vec3(def.targetOffsetX,
                           def.targetOffsetY,
                           def.targetOffsetZ);

  if(auto pl = gothic.player()) {
    Matrix4x4 rot;
    rot.identity();
    rot.rotateOY(90-pl->rotation());
    rot.project(targetOffset.x,targetOffset.y,targetOffset.z);
    }

  return pos + targetOffset;
  }

Vec3 Camera::applyModRotation(const Vec3& spin) {
  const auto& def = cameraDef();
  Vec3 rotOffset = {def.rotOffsetX,def.rotOffsetY,def.rotOffsetZ};
  if(camMod==Dialog)
    rotOffset = {}; // FIXME?
  return spin-rotOffset;
  }

Vec3 Camera::calcTranslation(float dist) const {
  Vec3 tr  = {0,0,dist};
  auto mTr = mkRotation(state.spin);
  mTr.inverse();
  mTr.project(tr);
  return tr;
  }

Matrix4x4 Camera::mkView(const Vec3& pos, float dist) const {
  static float scale = 0.0009f;

  auto tr = calcTranslation(dist);

  Matrix4x4 view;
  view.identity();
  view.scale(-1,-1,-1);

  view.mul(mkRotation(state.spin2));
  view.scale(scale);
  view.translate(-pos-tr);

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

const Daedalus::GEngineClasses::CCamSys &Camera::cameraDef() const {
  auto& camd = gothic.getCameraDef();
  if(camMod==Dialog)
    return camd.dialogCam();
  if(camMod==Normal)
    return camd.stdCam();
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
    const char *tag = "", *pos = nullptr;
    if(auto pl = gothic.player())
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

void Camera::implMove(Tempest::Event::KeyType key) {
  float dpos = 60.f;

  float k = -float(M_PI/180.0);
  float s = std::sin(state.spin.x*k), c=std::cos(state.spin.x*k);

  if(key==KeyEvent::K_A) {
    state.pos.x+=dpos*c;
    state.pos.z-=dpos*s;
    }
  if(key==KeyEvent::K_D) {
    state.pos.x-=dpos*c;
    state.pos.z+=dpos*s;
    }
  if(key==KeyEvent::K_W) {
    state.pos.x-=dpos*s;
    state.pos.z-=dpos*c;
    }
  if(key==KeyEvent::K_S){
    state.pos.x+=dpos*s;
    state.pos.z+=dpos*c;
    }
  if(auto world = gothic.world())
    state.pos.y = world->physic()->landRay(state.pos.x,state.pos.y,state.pos.z).v.y;
  }

void Camera::setPosition(float x, float y, float z) {
  dest.pos  = {x,y,z};
  state.pos = applyModPosition(dest.pos);
  }

void Camera::setDestPosition(float x, float y, float z) {
  dest.pos = {x,y,z};
  }

void Camera::setDialogDistance(float d) {
  dlgDist = d;
  }

void Camera::followPos(Vec3& pos, Vec3 dest, bool inMove, float dtF) {
  const auto& def = cameraDef();
  auto        dp  = (dest-pos);
  auto        len = dp.manhattanLength();

  if(len>0.1f && def.translate && camMod!=Dialog && camMod!=Mobsi){
    const float maxDist = 180;
    float       speed   = 0;
    if(inMove)
      speed = def.veloTrans*dtF; else
      speed = dp.manhattanLength()*dtF*2.f;
    float       tr      = std::min(speed,len);
    if(len-tr>maxDist)
      tr = (len-maxDist);

    float k = tr/len;
    pos = Vec3(pos.x+dp.x*k, pos.y+dp.y*k, pos.z+dp.z*k);
    } else {
    pos = dest;
    }
  }

void Camera::followAng(Vec3& spin, Vec3 dest, float dtF) {
  const auto& def = cameraDef();
  float shift = def.veloRot*45;
  followAng(spin.x,dest.x,shift*dtF);
  followAng(spin.y,dest.y,shift*dtF);
  }

void Camera::followAng(float& ang,float dest,float speed) {
  float da = angleMod(dest-ang);
  if(std::abs(da)<speed) {
    ang = dest;
    return;
    }

  float shift = 0;
  if(da>0)
    shift =  std::min(da,speed);
  if(da<0)
    shift = -std::min(-da,speed);

  static const float min=-45, max=45;
  if(da>max+1.f) {
    shift = (da-max);
    }
  if(da<min-1.f) {
    shift = (da-min);
    }
  ang += shift;
  }

void Camera::tick(const Npc& npc, uint64_t dt, bool inMove, bool includeRot) {
  const auto& def = cameraDef();
  const float dtF = float(dt)/1000.f;

  clampRange(dest.range);

  if(!hasPos) {
    state.range  = dest.range;
    dest.pos     = npc.cameraBone();
    state.pos    = applyModPosition(dest.pos);
    state.spin   = dest.spin;
    state.spin.x += def.bestAzimuth;
    hasPos = true;
    }

  if(gothic.isPause())
    return;

  {
  const float zSpeed = 5.f*dtF;
  const float dz = dest.range-state.range;
  if(std::fabs(dz)<zSpeed)
    state.range=dest.range;
  else if(state.range<dest.range)
    state.range+=zSpeed;
  else if(state.range>dest.range)
    state.range-=zSpeed;
  }

  {
  auto pos = applyModPosition(dest.pos);
  followPos(state.pos,pos,inMove,dtF);
  }

  if(includeRot) {
    auto rotation = dest.spin;
    followAng(state.spin, rotation,dtF);
    rotation = applyModRotation(rotation);
    followAng(state.spin2,rotation,dtF);

    if(state.spin.x>def.maxElevation)
      state.spin.x = def.maxElevation;
    if(state.spin.x<def.minElevation)
      ;//spin.x = def.minElevation;
    }
  }

void Camera::debugDraw(DbgPainter& p) {
  if(!dbg)
    return;

  auto destP = applyModPosition(dest.pos);

  p.setPen(Color(0,1,0));
  p.drawLine(destP, state.pos);

  auto& fnt = Resources::font();
  int   y   = 300+fnt.pixelSize();
  char buf[256] = {};

  std::snprintf(buf,sizeof(buf),"RaysCasted : %d", raysCasted);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"PlayerPos : %f %f %f", dest.pos.x, dest.pos.y, dest.pos.z);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"Range To Player : %f", state.range*100.f);
  p.drawText(8,y,buf); y += fnt.pixelSize();

  std::snprintf(buf,sizeof(buf),"Azimuth : %f", angleMod(dest.spin.y-state.spin.y));
  p.drawText(8,y,buf); y += fnt.pixelSize();
  std::snprintf(buf,sizeof(buf),"Elevation : %f", state.spin.x);
  p.drawText(8,y,buf); y += fnt.pixelSize();
  }

PointF Camera::spin() const {
  return PointF(state.spin.x,state.spin.y);
  }

PointF Camera::destSpin() const {
  return PointF(dest.spin.x,dest.spin.y);
  }

Matrix4x4 Camera::viewProj() const {
  Matrix4x4 ret=projective();
  if(auto w = gothic.world())
    w->globalFx()->morph(ret);
  ret.mul(view());
  return ret;
  }

Matrix4x4 Camera::view() const {
  const auto& def     = cameraDef();
  float       dist    = 0;

  if(camMod==Mobsi) {
    dist = def.maxRange;
    dist*=100; //to santimeters
    }
  else if(camMod==Dialog) {
    dist = dlgDist;
    }
  else {
    dist = state.range;
    if(dist<def.minRange)
      dist = def.minRange;
    if(dist>def.maxRange)
      dist = def.maxRange;
    dist*=100; //to santimeters
    }

  Matrix4x4 view=projective();
  if(auto w = gothic.world())
    w->globalFx()->morph(view);
  view.mul(mkView(state.pos,dist));

  float distMd = dist;
  if(def.collision)
    distMd = calcCameraColision(view,dist);
  view=mkView(state.pos,distMd);
  return view;
  }

float Camera::calcCameraColision(const Matrix4x4& view, const float dist) const {
  auto world = gothic.world();
  if(world==nullptr)
    return dist;

  float minDist = 20;
  float padding = 20;

  auto& physic = *world->physic();

  Matrix4x4 vinv=view;
  vinv.inverse();

  raysCasted = 0;
  float distMd = dist;
  auto  tr     = calcTranslation(dist);
  static int n = 1, nn=1;
  for(int i=-n;i<=n;++i)
    for(int r=-n;r<=n;++r) {
      float u = float(i)/float(nn),v = float(r)/float(nn);
      Tempest::Vec3 r0 = state.pos;
      Tempest::Vec3 r1 = {u,v,0};

      vinv.project(r1.x,r1.y,r1.z);

      auto rc = physic.ray(r0.x,r0.y,r0.z, r1.x,r1.y,r1.z);
      auto d  = rc.v;
      d -=r0;
      r1-=r0;

      float dist0 = r1.manhattanLength();
      float dist1 = Vec3::dotProduct(d,tr)/dist;
      if(rc.hasCol)
        dist1 = std::max<float>(0,dist1-padding);

      float md = dist-std::max(0.f,dist0-dist1);
      if(md<distMd)
        distMd=md;
      raysCasted++;
      }
  return std::max(minDist,distMd);
  }
