#include "camera.h"

#include "game/playercontrol.h"
#include "game/definitions/cameradefinitions.h"

#include "gothic.h"
#include "world/world.h"
#include "game/serialize.h"

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
  spin.y   = 0;
  camPos.y = 1000;
  }

void Camera::reset() {
  auto world = gothic.world();
  if(world==nullptr)
    return;
  auto pl = world->player();
  if(pl==nullptr)
    return;

  implReset(*pl);
  }

void Camera::implReset(const Npc &pl) {
  auto& def  = cameraDef();
  float tr   = pl.translateY();
  camPos     = pl.position();
  camBone    = pl.cameraBone();
  spin.x     = pl.rotation();
  spin.y     = 0;
  camPos.y  += tr + tr*(def.bestElevation-10)/20.f;
  }

void Camera::save(Serialize &s) {
  s.write(spin.x,spin.y,camPos,zoom,hasPos);
  }

void Camera::load(Serialize &s, Npc *pl) {
  if(pl)
    implReset(*pl);
  if(s.version()<1)
    return;
  if(s.version()<7) {
    uint8_t bug[3];
    s.read(spin.x,spin.y,bug,zoom,hasPos);
    return;
    }
  s.read(spin.x,spin.y,camPos,zoom,hasPos);
  }

void Camera::changeZoom(int delta) {
  const float dd=0.15f;
  if(delta>0)
    dist -= dd;
  if(delta<0)
    dist += dd;
  clampZoom(dist);
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
  camMod = m;
  }

void Camera::setSpin(const PointF &p) {
  spin = p;
  }

void Camera::setDistance(float d) {
  dist = d*0.01f;
  clampZoom(dist);
  }

Matrix4x4 Camera::viewShadow(const std::array<float,3>& ldir, int layer) const {
  const float scale = 0.0008f;
  const float c = std::cos(spin.x*float(M_PI)/180.f), s = std::sin(spin.x*float(M_PI)/180.f);

  Matrix4x4 view;
  if(ldir[1]>=0.f)
    return view;

  view.identity();

  if(layer>0)
    view.scale(0.2f);

  view.translate(0.f,0.5f,0.5f);
  view.rotate(/*spin.y*/90, 1, 0, 0);
  view.translate(0.f,0.f,0.f);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(scale,scale*0.3f,scale);
  view.translate(camPos.x,camPos.y,camPos.z);
  view.scale(-1,-1,-1);

  auto inv = view;
  inv.inverse();
  float cx=0,cy=0,cz=0;
  inv.project(cx,cy,cz);
  cy=camPos.y;

  float lx  = view.at(1,0);
  float ly  = view.at(1,1);
  float lz  = view.at(1,2);
  float k   = ldir[1]!=0.f ? lz/ldir[1] : 0.f;

  lx = -ldir[2]*k;
  ly = -ldir[0]*k;

  lz =  ldir[1]*k;

  float dx = lx*c-ly*s;
  float dy = lx*s+ly*c;

  view.set(1,0, dx);
  view.set(1,1, dy);
  view.set(1,2, lz);

  // float cx = camPos[0],
  //       cy = camPos[1],
  //       cz = camPos[2];
  view.project(cx,cy,cz);
  view.set(3,0, view.at(3,0)-cx);
  view.set(3,1, view.at(3,1)-cy);

  return view;
  }

Matrix4x4 Camera::mkView(float dist) const {
  const float scale=0.0009f;
  Matrix4x4 view;
  view.identity();
  view.translate(0,0,dist*scale);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(scale);
  view.translate(camPos.x,camPos.y,camPos.z);
  view.scale(-1,-1,-1);
  return view;
  }

const Daedalus::GEngineClasses::CCamSys &Camera::cameraDef() const {
  auto& camd = gothic.getCameraDef();
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
  return camd.stdCam();
  }

void Camera::clampZoom(float &zoom) {
  const auto& def = cameraDef();

  if(zoom<0 && zoom<def.minRange)
    zoom = def.bestRange;

  if(zoom>def.maxRange)
    zoom = def.maxRange;
  if(zoom<def.minRange)
    zoom = def.minRange;
  }

void Camera::implMove(Tempest::Event::KeyType key) {
  float dpos = 60.f/(zoom);

  float k = -float(M_PI/180.0);
  float s = std::sin(spin.x*k), c=std::cos(spin.x*k);

  if(key==KeyEvent::K_A) {
    camPos.x+=dpos*c;
    camPos.z-=dpos*s;
    }
  if(key==KeyEvent::K_D) {
    camPos.x-=dpos*c;
    camPos.z+=dpos*s;
    }
  if(key==KeyEvent::K_W) {
    camPos.x-=dpos*s;
    camPos.z-=dpos*c;
    }
  if(key==KeyEvent::K_S){
    camPos.x+=dpos*s;
    camPos.z+=dpos*c;
    }
  if(auto world = gothic.world())
    camPos.y = world->physic()->dropRay(camPos.x,camPos.y,camPos.z).y();
  }

void Camera::setPosition(float x, float y, float z) {
  camPos = {x,y,z};
  }

void Camera::follow(const Npc &npc,uint64_t dt,bool inMove,bool includeRot) {
  const auto& def = cameraDef();
  const float dtF = float(dt)/1000.f;

  clampZoom(dist);

  if(hasPos){
    auto  pos = npc.position();
    float tr  = npc.translateY();
    // min/best/max Elevation
    // normal:    0/30/90
    // inventory: 0/20/90
    pos.y += tr + tr*(def.bestElevation-10)/20.f; // HACK

    /*
    auto speed = npc.animMoveSpeed(dt);
    if(std::sqrt(speed.x*speed.x+speed.y*speed.y+speed.z*speed.z)>30*float(dt)/1000)
      isInMove = true; else
      isInMove = false;*/
    isInMove = inMove;

    auto dp  = (pos-camPos);
    auto len = dp.manhattanLength();

    if(len>0.1f && def.translate){
      const float maxDist = 100;
      float       speed   = isInMove ? 0.f : def.veloTrans*dtF*10.f;
      float       tr      = std::min(speed,len);
      if(len-tr>maxDist)
        tr += (len-maxDist);

      float k = tr/len;
      setPosition(camPos.x+dp.x*k, camPos.y+dp.y*k, camPos.z+dp.z*k);
      }
    } else {
    reset();
    hasPos   = true;
    isInMove = false;
    }

  if(includeRot && def.rotate!=0) {
    float angle = npc.rotation();
    float da    = angleMod(spin.x-angle);
    float min   = def.minAzimuth+180;
    float max   = def.maxAzimuth-180;

    float shift = def.veloRot*dtF*60.f; // my guess: speed is angle per frame
    if(da<min)
      shift = std::max(shift,+(min-da));
    if(da>max)
      shift = std::max(shift,-(max-da));

    if(da>0){
      shift = -std::min(+da,shift);
      } else {
      shift = +std::min(-da,shift);
      }
    spin.x += shift;
    }
  }

Matrix4x4 Camera::view() const {
  const float dist    = this->dist*100.f/zoom;
  const float minDist = 25;

  auto world = gothic.world();
  if(world==nullptr)
    return mkView(dist);

  const auto proj = world->view()->projective();

  Matrix4x4 view=proj;
  view.mul(mkView(dist));

  Matrix4x4 vinv=view;
  vinv.inverse();

  float distMd = dist;

  static int n = 1, nn=1;
  for(int i=-n;i<=n;++i)
    for(int r=-n;r<=n;++r) {
      float u = float(i)/float(nn),v = float(r)/float(nn);
      Tempest::Vec3 r0=camPos;
      Tempest::Vec3 r1={u,v,0};

      view.project(r0.x,r0.y,r0.z);
      //r0[0] = u;
      //r0[1] = v;

      vinv.project(r0.x,r0.y,r0.z);
      vinv.project(r1.x,r1.y,r1.z);

      auto d = world->physic()->ray(r0.x,r0.y,r0.z, r1.x,r1.y,r1.z).v;
      d.x-=r0.x;
      d.y-=r0.y;
      d.z-=r0.z;

      r1-=r0;

      float dist0 = r1.manhattanLength();
      float dist1 = d.manhattanLength();

      float md = std::max(dist-std::max(0.f,dist0-dist1),minDist);
      if(md<distMd)
        distMd=md;
      }

  view=mkView(distMd);
  return view;
  }
