#include "physicmeshshape.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "dynamicworld.h"

float PhysicMeshShape::friction() const {
  return frict;
  }

PhysicMeshShape* PhysicMeshShape::load(PackedMesh&& packed) {
  uint32_t count = 0;
  for(auto& i : packed.subMeshes)
    if(!i.material.disable_collision && i.iboLength > 0) {
      count++;
      }
  if(count == 0)
    return nullptr;
  return new PhysicMeshShape(std::move(packed));
  }

PhysicMeshShape* PhysicMeshShape::load(const PhysicMeshShape& src, const std::unordered_set<std::string>& noColl) {
  std::vector<bool> keep(src.material.size());
  bool              any = false;
  for(size_t i=0; i<keep.size(); ++i) {
    keep[i] = (noColl.find(src.material[i])==noColl.end());
    any    |= keep[i];
    }
  if(!any)
    return nullptr;
  return new PhysicMeshShape(src,keep);
  }

PhysicMeshShape::PhysicMeshShape(PackedMesh&& sPacked)
  :mesh(std::move(sPacked)), shape(&mesh, mesh.useQuantization(), true) {
  for(auto& i:sPacked.subMeshes)
    frict += DynamicWorld::materialFriction(i.material.group);
  frict = frict/float(sPacked.subMeshes.size());
  for(auto& i:sPacked.subMeshes)
    if(!i.material.disable_collision && i.iboLength>0)
      material.push_back(i.material.name);
  }

PhysicMeshShape::PhysicMeshShape(const PhysicMeshShape& src, const std::vector<bool>& keep)
  :mesh(src.mesh,keep), shape(&mesh, mesh.useQuantization(), true), frict(src.frict) {
  for(size_t i=0; i<keep.size(); ++i)
    if(keep[i])
      material.push_back(src.material[i]);
  }

bool PhysicMeshShape::hasMaterial(const std::unordered_set<std::string>& names) const {
  for(auto& i:material)
    if(names.find(i)!=names.end())
      return true;
  return false;
  }
