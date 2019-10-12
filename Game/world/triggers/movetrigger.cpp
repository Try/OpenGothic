#include "movetrigger.h"

#include "world/world.h"

MoveTrigger::MoveTrigger(ZenLoad::zCVobData&& d,World& owner)
  :AbstractTrigger(std::move(d),owner) {
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

void MoveTrigger::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  }

void MoveTrigger::setPhysic(DynamicWorld::StaticItem &&p) {
  physic = std::move(p);
  }

void MoveTrigger::setObjMatrix(const Tempest::Matrix4x4 &m) {
  pos = m;
  view  .setObjMatrix(m);
  physic.setObjMatrix(m);
  }

void MoveTrigger::onTrigger(const TriggerEvent&) {
  if(data.zCMover.keyframes.size()==0)
    return;

  if(frame==0)
    frame=data.zCMover.keyframes.size()-1; else
    frame=0;

  auto m = pos;
  m.set(3,0, data.zCMover.keyframes[frame].pos.x);
  m.set(3,1, data.zCMover.keyframes[frame].pos.y);
  m.set(3,2, data.zCMover.keyframes[frame].pos.z);
  setObjMatrix(m);
  }

bool MoveTrigger::hasVolume() const {
  return false;
  }
