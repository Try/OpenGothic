#include "camera.h"

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
      camPos = pl->position();
      spin.x = pl->rotation();
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

Matrix4x4 Camera::view() const {
  Matrix4x4 view;
  view.identity();
  view.translate(0,0,0.3f/zoom);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(0.0008f);
  view.translate(camPos[0],camPos[1]+180,camPos[2]);
  view.scale(-1,-1,-1);
  return view;
  }

void Camera::implMove(Tempest::Event::KeyType key) {
  if(world!=nullptr && world->player()!=nullptr)
    return implMovePl(key);

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

void Camera::implMovePl(Event::KeyType key) {
  Npc&  pl = *world->player();
  float speed = 60.f, rspeed=5.f;

  float k = -float(M_PI/180.0), rot = pl.rotation();
  float s = std::sin(rot*k), c=std::cos(rot*k);

  auto dpos=pl.position();
  if(key==KeyEvent::K_Q){
    rot += rspeed;
    }
  if(key==KeyEvent::K_E){
    rot -= rspeed;
    }
  if(key==KeyEvent::K_A) {
    dpos[0]+=speed*c;
    dpos[2]-=speed*s;
    }
  if(key==KeyEvent::K_D) {
    dpos[0]-=speed*c;
    dpos[2]+=speed*s;
    }
  if(key==KeyEvent::K_W) {
    dpos[0]-=speed*s;
    dpos[2]-=speed*c;
    }
  if(key==KeyEvent::K_S){
    dpos[0]+=speed*s;
    dpos[2]+=speed*c;
    }
  dpos[1] = world->physic()->dropRay(dpos[0],dpos[1],dpos[2]);
  pl.setPosition(dpos);
  pl.setDirection(rot);

  spin.x = rot;
  camPos[0]=dpos[0];
  camPos[1]=dpos[1];
  camPos[2]=dpos[2];
  }
