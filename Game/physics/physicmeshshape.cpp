#include "physicmeshshape.h"

PhysicMeshShape *PhysicMeshShape::load(ZenLoad::PackedMesh&& sPacked) {
  uint32_t count=0;
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0){
      count++;
      }
  if(count==0)
    return nullptr;
  return new PhysicMeshShape(std::move(sPacked));
  }

PhysicMeshShape::PhysicMeshShape(ZenLoad::PackedMesh&& sPacked)
  :mesh(std::move(sPacked)), shape(&mesh,true,true){
  }
