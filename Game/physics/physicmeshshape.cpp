#include "physicmeshshape.h"

PhysicMeshShape *PhysicMeshShape::load(const ZenLoad::PackedMesh &sPacked) {
  uint32_t count=0;
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0){
      count++;
      }
  if(count==0)
    return nullptr;
  return new PhysicMeshShape(sPacked);
  }

PhysicMeshShape::PhysicMeshShape(const ZenLoad::PackedMesh& sPacked)
  :mesh(sPacked), shape(&mesh,true,true){
  }
