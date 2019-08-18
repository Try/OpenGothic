#include "bullet.h"

#include "world.h"

Bullet::Bullet(World& owner, size_t itemInstance)
  :owner(owner){
  // FIXME: proper item creation
  Daedalus::GEngineClasses::C_Item  hitem={};
  owner.script().initializeInstance(hitem,itemInstance);
  owner.script().clearReferences(hitem);

  setView(owner.getStaticView(hitem.visual,hitem.material));
  }

Bullet::~Bullet() {
  }

void Bullet::setPosition(float x, float y, float z) {
  pos = {x,y,z};
  updateMatrix();
  }

void Bullet::setDirection(float x, float y, float z) {
  dir = {x,y,z};
  ang = std::atan2(z,x)*float(180/M_PI)+180.f;
  }

void Bullet::setView(StaticObjects::Mesh &&m) {
  view = std::move(m);
  view.setObjMatrix(mat);
  updateMatrix();
  }

void Bullet::tick(uint64_t dt) {
  float  k = dt/1000.f;
  pos[0] += dir[0]*k;
  pos[1] += dir[1]*k;
  pos[2] += dir[2]*k;
  updateMatrix();
  }

void Bullet::updateMatrix() {
  mat.identity();
  mat.translate(pos[0],pos[1],pos[2]);
  mat.rotateOY(-ang);
  view.setObjMatrix(mat);
  }
