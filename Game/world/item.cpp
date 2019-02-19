#include "item.h"

#include "worldscript.h"

Item::Item(WorldScript& owner,Daedalus::GameState::ItemHandle hitem)
  :owner(owner),hitem(hitem){
  }

void Item::setView(StaticObjects::Mesh &&m) {
  view = std::move(m);
  updateMatrix();
  }

void Item::setPosition(float x, float y, float z) {
  pos={{x,y,z}};
  updateMatrix();
  }

void Item::setDirection(float, float, float) {
  }

void Item::setMatrix(const Tempest::Matrix4x4 &m) {
  pos[0] = m.at(3,0);
  pos[1] = m.at(3,1);
  pos[2] = m.at(3,2);
  view.setObjMatrix(m);
  }

const char *Item::displayName() const {
  return owner.vmItem(hitem).name.c_str();
  }

std::array<float,3> Item::position() const {
  return pos;
  }

void Item::updateMatrix() {
  Tempest::Matrix4x4 m;
  m.identity();
  m.translate(pos[0],pos[1],pos[2]);
  view.setObjMatrix(m);
  }
