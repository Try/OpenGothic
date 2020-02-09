#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include "physicmesh.h"

PhysicMesh::PhysicMesh(const ZenLoad::PackedMesh &sPacked)
  :PhysicMesh(sPacked.vertices) {
  for(auto& i:sPacked.subMeshes)
    if(!i.material.noCollDet && i.indices.size()>0)
      addIndex(i.indices,i.material.matGroup);
  }

PhysicMesh::PhysicMesh(const std::vector<ZMath::float3>& v, const std::vector<uint32_t>& index)
  :PhysicMesh(v){
  addIndex(index,0);
  }

PhysicMesh::PhysicMesh(const std::vector<ZMath::float3>& v)
  :vert(v.size()) {
  for(size_t i=0;i<v.size();++i){
    vert[i].setValue(v[i].x,v[i].y,v[i].z);
    }
  }

PhysicMesh::PhysicMesh(const std::vector<ZenLoad::WorldVertex> &v)
  :vert(v.size()) {
  for(size_t i=0;i<v.size();++i){
    vert[i].setValue(v[i].Position.x,v[i].Position.y,v[i].Position.z);
    }
  }

void PhysicMesh::addIndex(const std::vector<uint32_t> &index, uint8_t material) {
  if(index.size()==0)
    return;

  size_t off=id.size();
  id.resize(off+index.size());
  for(size_t i=0;i<index.size();i+=3){
    id[off+i+0] = index[i+0];
    id[off+i+1] = index[i+2];
    id[off+i+2] = index[i+1];
    }

  btIndexedMesh meshIndex={};
  meshIndex.m_numTriangles = id.size()/3;
  meshIndex.m_numVertices  = int32_t(vert.size());

  meshIndex.m_indexType           = PHY_INTEGER;
  meshIndex.m_triangleIndexBase   = reinterpret_cast<const uint8_t*>(&id[0]);
  meshIndex.m_triangleIndexStride = 3 * sizeof(uint32_t);

  meshIndex.m_vertexBase          = reinterpret_cast<const uint8_t*>(&vert[0]);
  meshIndex.m_vertexStride        = sizeof(btVector3);

  m_indexedMeshes.push_back(meshIndex);
  segments.push_back(Segment{off,int(index.size()/3),material});
  adjustMesh();
  }

uint8_t PhysicMesh::getMaterialId(size_t segment) const {
  if(segment<segments.size())
    return segments[segment].mat;
  return 0;
  }

bool PhysicMesh::useQuantization() const {
  return segments.size()<1024;
  }

void PhysicMesh::adjustMesh(){
  for(int i=0;i<m_indexedMeshes.size();++i){
    btIndexedMesh& meshIndex=m_indexedMeshes[i];
    Segment&       sg       =segments[size_t(i)];

    meshIndex.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(&id[sg.off]);
    meshIndex.m_numTriangles      = sg.size;
    }
  }
