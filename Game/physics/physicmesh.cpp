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

ProtoMesh PhysicMesh::decalMesh(const std::string& tex, float x, float y, float z,
                                float sX, float sY, float sZ) const {
  sX*=0.5f;
  sZ*=0.5f;

  struct {
    std::vector<Resources::Vertex> v;
    std::vector<uint32_t>          i;
    } d;

  std::vector<uint32_t> idx;
  for(size_t i=0;i<id.size();i+=3) {
    auto& a = vert[id[i+0]];
    auto& b = vert[id[i+1]];
    auto& c = vert[id[i+2]];

    if(!intersects(a,b,c, x,y,z,sX,sY,sZ))
      continue;

    idx.push_back(id[i+0]);
    idx.push_back(id[i+1]);
    idx.push_back(id[i+2]);
    }

  for(size_t i=0;i<idx.size();i+=3) {
    std::swap(idx[i+1],idx[i+2]);
    }

  std::vector<uint32_t> used;
  for(uint32_t i:idx) {
    bool skip=false;
    for(size_t r=0;r<used.size();++r) {
      if(used[r]==i) {
        skip=true;
        d.i.push_back(d.i[r]);
        used.push_back(i);
        break;
        }
      }

    if(skip)
      continue;

    auto& vx = vert[i];
    Resources::Vertex v={};
    v.pos[0]  = vx.x();
    v.pos[1]  = vx.y();
    v.pos[2]  = vx.z();

    v.norm[0] = 0.f;
    v.norm[1] = 1.f;
    v.norm[2] = 0.f;

    v.uv[0]   = 0.5f+0.5f*(v.pos[0]-x)/sX;
    v.uv[1]   = 0.5f+0.5f*(v.pos[2]-z)/sZ;

    v.color   = 0xFFFFFFFF;

    used.push_back(i);
    d.i.push_back(d.v.size());
    d.v.push_back(v);
    }

  return ProtoMesh(tex, std::move(d.v), std::move(d.i));
  }

void PhysicMesh::adjustMesh(){
  for(int i=0;i<m_indexedMeshes.size();++i){
    btIndexedMesh& meshIndex=m_indexedMeshes[i];
    Segment&       sg       =segments[size_t(i)];

    meshIndex.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(&id[sg.off]);
    meshIndex.m_numTriangles      = sg.size;
    }
  }

bool PhysicMesh::intersects(btVector3 a, btVector3 b, btVector3 c,
                            float x, float y, float z, float sX, float sY, float sZ) {
  for(auto v:{&a,&b,&c}) {
    // move to from center
    v->setX(v->x()-x);
    v->setY(v->y()-y);
    v->setZ(v->z()-z);
    }

  float x0 = std::min(a.x(),std::min(b.x(),c.x()));
  float y0 = std::min(a.y(),std::min(b.y(),c.y()));
  float z0 = std::min(a.z(),std::min(b.z(),c.z()));

  float x1 = std::max(a.x(),std::max(b.x(),c.x()));
  float y1 = std::max(a.y(),std::max(b.y(),c.y()));
  float z1 = std::max(a.z(),std::max(b.z(),c.z()));

  if(x1<-sX || sX<x0)
    return false;
  if(y1<-sY || sY<y0)
    return false;
  if(z1<-sZ || sZ<z0)
    return false;

  return true;
  }
