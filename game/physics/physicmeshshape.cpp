#include "physicmeshshape.h"

#include "dynamicworld.h"

PhysicMeshShape *PhysicMeshShape::load(ZenLoad::PackedMesh&& sPacked) {
  uint32_t count=0;
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indexSize>0) {
      count++;
      }
  if(count==0)
    return nullptr;
  return new PhysicMeshShape(std::move(sPacked));
  }

PhysicMeshShape::PhysicMeshShape(ZenLoad::PackedMesh&& sPacked)
  :mesh(std::move(sPacked)) {
  uint32_t count=0;
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indexSize>0) {
      friction += DynamicWorld::materialFriction(ZenLoad::MaterialGroup(i.material.matGroup));
      ++count;
      }
  friction = friction/float(count);
  }
