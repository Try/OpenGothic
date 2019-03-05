#include "movetrigger.h"

#include "world.h"

MoveTrigger::MoveTrigger(ZenLoad::zCVobData&& d,World& owner)
  :Trigger(std::move(d)) {
  float v[16]={};
  std::memcpy(v,data.worldMatrix.m,sizeof(v));

  setView(owner.getView(data.visual));
  if(data.cdDyn || data.cdStatic) {
    auto mesh   = Resources::loadMesh(data.visual);
    auto physic = Resources::physicMesh(mesh);
    setPhysic(owner.physic()->staticObj(physic,Tempest::Matrix4x4(v)));
    }
  setObjMatrix(Tempest::Matrix4x4(v));
  }

void MoveTrigger::setView(StaticObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::setPhysic(DynamicWorld::Item &&p) {
  physic = std::move(p);
  }

void MoveTrigger::setObjMatrix(const Tempest::Matrix4x4 &m) {
  pos = m;
  view  .setObjMatrix(m);
  physic.setObjMatrix(m);
  }

void MoveTrigger::onTrigger() {
  if(data.zCMover.keyframes.size()==0)
    return;

  frame = (frame+1)%data.zCMover.keyframes.size();

  auto m = pos;
  m.set(3,0, data.zCMover.keyframes[frame].pos.x);
  m.set(3,1, data.zCMover.keyframes[frame].pos.y);
  m.set(3,2, data.zCMover.keyframes[frame].pos.z);
  setObjMatrix(m);
  }
