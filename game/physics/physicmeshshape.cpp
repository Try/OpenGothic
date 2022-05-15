#include "physicmeshshape.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "dynamicworld.h"

float PhysicMeshShape::friction() const {
  return frict;
  }

PhysicMeshShape* PhysicMeshShape::load(PackedMesh&& packed) {
  uint32_t count=0;
  for(auto& i:packed.subMeshes)
    if(!i.material.noCollDet && i.iboLength>0){
      count++;
      }
  if(count==0)
    return nullptr;
  return new PhysicMeshShape(std::move(packed));
  }

PhysicMeshShape::PhysicMeshShape(PackedMesh&& sPacked)
  :mesh(std::move(sPacked)), shape(&mesh,true,true) {
  for(auto& i:sPacked.subMeshes)
    frict += DynamicWorld::materialFriction(ZenLoad::MaterialGroup(i.material.matGroup));
  frict = frict/float(sPacked.subMeshes.size());
  }
