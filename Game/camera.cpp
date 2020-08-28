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

  state.pos    = pl.cameraBone();
  state.spin.x = pl.rotation();
  state.spin.y = 0;
  dest         = state;

  applyModRotation(state.spin);

  zoom         = def.bestRange/def.maxRange;
  }

void Camera::save(Serialize &s) {
  s.write(state.spin,state.pos,
          dest.spin,dest.pos,
          zoom,hasPos);
  }

void Camera::load(Serialize &s, Npc *pl) {
  if(pl)
    implReset(*pl);
  if(s.version()<8)
    return;
  s.read(state.spin,state.pos,
         dest.spin,dest.pos,
         zoom,hasPos);
  }

void Camera::changeZoom(int delta) {
  const float dd=1.1f;
  if(delta>0)
    zoom/=dd;
  if(delta<0)
    zoom*=dd;
  clampZoom(zoom);
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
  if(camMod==Mode::Inventory ||
     camMod==Mode::Dialog ||
     camMod==Mode::Death) {
    const auto& def = cameraDef();
    dest.spin.x = def.bestAzimuth;
    if(auto pl=gothic.player())
      dest.spin.x+=pl->rotation();

    if(camMod!=Dialog)
      dest.spin.y = def.bestElevation;
    }
  }

void Camera::setSpin(const PointF &p) {
  state.spin = Vec3(p.x,p.y,0);
  dest.spin  = state.spin;

  applyModRotation(state.spin);
  }

void Camera::setDestSpin(const PointF& p) {
  dest.spin = Vec3(p.x,p.y,0);
  if(dest.spin.y<-90)
    dest.spin.y = -90;
  if(dest.spin.y>90)
    dest.spin.y = 90;
  }

void Camera::onRotateMouse(const PointF& dpos) {
  dest.spin.x += dpos.x;
  dest.spin.y += dpos.y;
  }

Matrix4x4 Camera::viewShadow(const Vec3& ldir, int layer) const {
  const float scale = 0.0008f;
  const float c = std::cos(state.spin.x*float(M_PI)/180.f), s = std::sin(state.spin.x*float(M_PI)/180.f);

  Matrix4x4 view;
  if(ldir.y>=0.f)
    return view;

  view.identity();

  if(layer>0)
    view.scale(0.2f);

  view.translate(0.f,0.5f,0.5f);
  view.rotate(/*spin.y*/90, 1, 0, 0);
  view.translate(0.f,0.f,0.f);
  view.rotate(state.spin.x, 0, 1, 0);
  view.scale(scale,scale*0.3f,scale);
  view.translate(state.pos.x,state.pos.y,state.pos.z);
  view.scale(-1,-1,-1);

  auto inv = view;
  inv.inverse();
  float cx=0,cy=0,cz=0;
  inv.project(cx,cy,cz);
  cy=state.pos.y;

  float lx  = view.at(1,0);
  float ly  = view.at(1,1);
  float lz  = view.at(1,2);
  float k   = ldir.y!=0.f ? lz/ldir.y : 0.f;

  lx = -ldir.z*k;
  ly = -ldir.x*k;

  lz =  ldir.y*k;

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

void Camera::applyModPosition(Vec3& pos) {
  const auto& def = cameraDef();

  Vec3 rotOffset = Vec3(def.targetOffsetX/*+20*/,
                        def.targetOffsetY,
                        def.targetOffsetZ);
  if(auto pl = gothic.player()) {
    Matrix4x4 rot;
    rot.identity();
    rot.rotateOY(90-pl->rotation());
    rot.project(rotOffset.x,rotOffset.y,rotOffset.z);
    }
  pos+=rotOffset;
  }

void Camera::applyModRotation(Vec3& spin) {
  const auto& def = cameraDef();
  // spin.x += def.bestAzimuth;
  // if(camMod!=Dialog)
  //   spin.y += def.bestElevation;

  if(camMod!=Dialog) {
    spin.y += def.rotOffsetX; //NOTE: rotOffsetX is upside-down rotation
    spin.x += def.rotOffsetY;
    spin.z += def.rotOffsetZ;
    }
  }

Matrix4x4 Camera::mkView(float dist) const {
  const float scale=0.0009f;
  Matrix4x4 view;
  view.identity();
  view.translate(0,0,dist*scale);
  view.rotate(state.spin.y, 1, 0, 0);
  view.rotate(state.spin.x, 0, 1, 0);
  view.rotate(state.spin.z, 0, 0, 1);
  view.scale(scale);
  view.translate(state.pos.x,state.pos.y,state.pos.z);
  view.scale(-1,-1,-1);
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

void Camera::clampZoom(float &zoom) {
  if(zoom>1)
    zoom = 1;
  if(zoom<0)
    zoom = 0;
  }

void Camera::implMove(Tempest::Event::KeyType key) {
  float dpos = 60.f/(zoom);

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
    state.pos.y = world->physic()->dropRay(state.pos.x,state.pos.y,state.pos.z).y();
  }

void Camera::setPosition(float x, float y, float z) {
  state.pos = {x,y,z};
  dest.pos  = state.pos;
  applyModPosition(state.pos);
  }

void Camera::setDestPosition(float x, float y, float z) {
  dest.pos = {x,y,z};
  }

void Camera::setDialogDistance(float d) {
  dlgDist = d;
  }

static void followAng(float& ang,float dest,float speed) {
  float da = angleMod(dest-ang);
  if(std::abs(da)<speed) {
    ang = dest;
    return;
    }

  float shift = 0;
  if(da>0)
    shift = std::min(da,speed);
  if(da<0)
    shift = -std::min(-da,speed);

  static const float min=-45, max=45;
  if(da>max)
    shift = (da-max);
  if(da<min)
    shift = (da-min);

  ang += shift;
  }

void Camera::follow(const Npc &npc,uint64_t dt,bool inMove,bool includeRot) {
  const auto& def = cameraDef();
  const float dtF = float(dt)/1000.f;

  clampZoom(zoom);

  if(!hasPos) {
    dest.pos     = npc.cameraBone();
    state.pos    = dest.pos;
    applyModPosition(state.pos);
    state.spin   = dest.spin;
    state.spin.x += def.bestAzimuth;
    hasPos = true;
    }

  {
  auto pos = dest.pos;
  applyModPosition(pos);
  auto dp  = (pos-state.pos);
  auto len = dp.manhattanLength();

  if(len>0.1f && def.translate && camMod!=Dialog){
    const float maxDist = 100;
    float       speed   = inMove ? 0.f : dp.manhattanLength()*dtF*(def.veloTrans/10.f);
    float       tr      = std::min(speed,len);
    if(len-tr>maxDist)
      tr += (len-maxDist);

    float k = tr/len;
    state.pos   = Vec3(state.pos.x+dp.x*k, state.pos.y+dp.y*k, state.pos.z+dp.z*k);
    camDistLast = (pos-state.pos).manhattanLength();
    } else {
    state.pos = pos;
    }
  }

  if(includeRot) {
    auto rotation = dest.spin;
    applyModRotation(rotation);

    float shift = def.veloRot*45;
    followAng(state.spin.x,rotation.x,shift*dtF);
    followAng(state.spin.y,rotation.y,shift*dtF);

    if(state.spin.y>def.maxElevation)
      state.spin.y = def.maxElevation;
    if(state.spin.y<def.minElevation)
      ;//spin.y = def.minElevation;
    }
  }

PointF Camera::spin() const {
  return PointF(state.spin.x,state.spin.y);
  }

PointF Camera::destSpin() const {
  return PointF(dest.spin.x,dest.spin.y);
  }

Matrix4x4 Camera::view() const {
  const auto& def     = cameraDef();
  float       dist    = 0;
  float       minDist = 25;

  if(camMod==Mobsi) {
    dist = def.maxRange;
    dist*=100; //to santimeters
    }
  else if(camMod==Dialog) {
    dist = dlgDist;
    }
  else {
    dist    = zoom*def.maxRange;
    if(dist<def.minRange)
      dist = def.minRange;
    if(dist>def.maxRange)
      dist = def.maxRange;
    dist*=100; //to santimeters
    }

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
      Tempest::Vec3 r0=state.pos;
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
