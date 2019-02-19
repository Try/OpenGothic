#include "camera.h"

#include "game/playercontrol.h"
#include "world/world.h"

using namespace Tempest;

Camera::Camera() {
  spin.y    = 0;
  camPos[1] = 1000;
  }

void Camera::setWorld(const World *w) {
  world = w;
  if(world){
    auto pl = world->player();
    if(pl) {
      camPos  = pl->position();
      camBone = pl->cameraBone();
      spin.x  = pl->rotation();
      }
    }
  }

void Camera::changeZoom(int delta) {
  if(delta>0)
    zoom *= 1.1f;
  if(delta<0)
    zoom /= 1.1f;
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

void Camera::setSpin(const PointF &p) {
  spin = p;
  }

static float length(const std::array<float,3>& d){
  return std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
  }

Matrix4x4 Camera::view() const {
  const float dist    = 375;
  const float minDist = 55;

  if(world==nullptr)
    return mkView(dist);

  const auto proj = world->view()->projective();

  Matrix4x4 view=proj;//mkView(dist);
  view.mul(mkView(dist));

  Matrix4x4 vinv=view;
  vinv.inverse();

  float distMd = dist;
  float u = 0,v=0;
  std::array<float,3> r0={u,v,0.65f};
  std::array<float,3> r1={u,v,0};

  vinv.project(r0[0],r0[1],r0[2]);
  vinv.project(r1[0],r1[1],r1[2]);

  auto d = world->physic()->ray(r0[0],r0[1],r0[2], r1[0],r1[1],r1[2]);
  d[0]-=r0[0];
  d[1]-=r0[1];
  d[2]-=r0[2];

  r1[0]-=r0[0];
  r1[1]-=r0[1];
  r1[2]-=r0[2];

  float dist0 = length(r1);
  float dist1 = length(d);

  float md = std::max(dist-std::max(0.f,dist0-dist1),minDist);
  if(md<distMd)
    distMd=md;

  view=mkView(distMd);
  return view;
  }

Matrix4x4 Camera::mkView(float dist) const {
  const float scale=0.0008f;
  Matrix4x4 view;
  view.identity();
  view.translate(0,0,dist*scale/zoom);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(scale);
  view.translate(camPos[0],camPos[1]+180,camPos[2]);
  //view.translate(camPos[0],-camBone[1],camPos[2]);
  view.scale(-1,-1,-1);
  return view;
  }

void Camera::implMove(Tempest::Event::KeyType key) {
  float dpos = 60.f/(zoom);

  float k = -float(M_PI/180.0);
  float s = std::sin(spin.x*k), c=std::cos(spin.x*k);

  if(key==KeyEvent::K_A) {
    camPos[0]+=dpos*c;
    camPos[2]-=dpos*s;
    }
  if(key==KeyEvent::K_D) {
    camPos[0]-=dpos*c;
    camPos[2]+=dpos*s;
    }
  if(key==KeyEvent::K_W) {
    camPos[0]-=dpos*s;
    camPos[2]-=dpos*c;
    }
  if(key==KeyEvent::K_S){
    camPos[0]+=dpos*s;
    camPos[2]+=dpos*c;
    }
  if(world)
    camPos[1] = world->physic()->dropRay(camPos[0],camPos[1],camPos[2]);
  }

void Camera::follow(const Npc &npc,bool includeRot) {
  if(hasPos){
    auto pos = npc.position();
    auto dx  = (pos[0]-camPos[0]);
    auto dy  = (pos[1]-camPos[1]);
    auto dz  = (pos[2]-camPos[2]);
    auto len = std::sqrt(dx*dx+dy*dy+dz*dz);

    if(len>0.1f){
      float k = 1.f;//0.25f;
      camPos[0] += dx*k;
      camPos[1] += dy*k;
      camPos[2] += dz*k;
      }
    } else {
    camPos = npc.position();
    hasPos = true;
    }

  if(includeRot)
    spin.x += (npc.rotation()-spin.x)/2;
  }
