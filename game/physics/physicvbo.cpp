#include "physicvbo.h"

using namespace reactphysics3d;

PhysicVbo::PhysicVbo(ZenLoad::PackedMesh&& sPacked)
  :PhysicVbo(sPacked.vertices) {
  id = std::move(sPacked.indices);
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indexSize>0) {
      addSegment(i.indexSize,i.indexOffset,i.material.matGroup,nullptr);
      }
  for(size_t i=0;i<id.size();i+=3){
    std::swap(id[i+1],id[i+2]);
    }
  adjustMesh();
  }

PhysicVbo::PhysicVbo(const std::vector<ZenLoad::WorldVertex>& v)
  :vStorage(v.size()), vert(vStorage) {
  for(size_t i=0;i<v.size();++i){
    vStorage[i] = Tempest::Vec3(v[i].Position.x,v[i].Position.y,v[i].Position.z);
    }
  }

PhysicVbo::PhysicVbo(const std::vector<Tempest::Vec3>* v)
  :vert(*v) {
  }

void PhysicVbo::addIndex(std::vector<uint32_t>&& index, uint8_t material) {
  addIndex(std::move(index),material,nullptr);
  }

void PhysicVbo::addIndex(std::vector<uint32_t>&& index, uint8_t material, const char* sector) {
  if(index.size()==0)
    return;

  size_t off    = id.size();
  size_t idSize = index.size();
  if(id.size()==0) {
    id=std::move(index);
    for(size_t i=0;i<id.size();i+=3){
      std::swap(id[i+1],id[i+2]);
      }
    } else {
    id.resize(off+index.size());
    for(size_t i=0;i<index.size();i+=3){
      id[off+i+0] = index[i+0];
      id[off+i+1] = index[i+2];
      id[off+i+2] = index[i+1];
      }
    }

  addSegment(idSize,off,material,sector);
  }

void PhysicVbo::addSegment(size_t indexSize, size_t offset, uint8_t material, const char* sector) {
  Segment sgm;
  sgm.off    = offset;
  sgm.size   = indexSize;
  sgm.mat    = material;
  sgm.sector = sector;
  segments.push_back(sgm);
  }

uint8_t PhysicVbo::materialId(size_t segment) const {
  if(segment<segments.size())
    return segments[segment].mat;
  return 0;
  }

const char* PhysicVbo::sectorName(size_t segment) const {
  if(segment<segments.size())
    return segments[segment].sector;
  return nullptr;
  }

bool PhysicVbo::useQuantization() const {
  return segments.size()<1024;
  }

bool PhysicVbo::isEmpty() const {
  return segments.size()==0;
  }

void PhysicVbo::adjustMesh() {
  vba.clear();
  for(size_t i=0; i<segments.size(); ++i) {
    auto& sg = segments[i];
    vba.emplace_back(vert.size(), vert.data(), sizeof(Tempest::Vec3),
                     sg.size/3,   &id[sg.off], sizeof(id[0])*3,
                     TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                     TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);
    }
  }

CollisionShape* PhysicVbo::mkMesh(reactphysics3d::PhysicsCommon& common) const {
  reactphysics3d::TriangleMesh* mesh = common.createTriangleMesh();
  for(auto& sg:vba)
    mesh->addSubpart(&sg);
  auto ret = common.createConcaveMeshShape(mesh);
  ret->setRaycastTestType(TriangleRaycastSide::FRONT);
  return ret;
  }

const char* PhysicVbo::validateSectorName(const char* name) const {
  for(auto& i:segments)
    if(std::strcmp(i.sector,name)==0)
      return i.sector;
  return nullptr;
  }
