#include "pfxemittermesh.h"

#include <Tempest/Log>

PfxEmitterMesh::PfxEmitterMesh(const ZenLoad::PackedMesh& src) {
  vertices.resize(src.vertices.size());
  for(size_t i=0;i<vertices.size();++i) {
    vertices[i].x = src.vertices[i].Position.x;
    vertices[i].y = src.vertices[i].Position.y;
    vertices[i].z = src.vertices[i].Position.z;
    }

  for(auto& mesh:src.subMeshes) {
    for(size_t i=0;i<mesh.indices.size();i+=3) {
      Triangle t;
      t.id[0] = mesh.indices[i+0];
      t.id[1] = mesh.indices[i+1];
      t.id[2] = mesh.indices[i+2];
      triangle.push_back(t);
      }
    }

  for(auto& t:triangle) {
    auto& a = vertices[t.id[0]];
    auto& b = vertices[t.id[1]];
    auto& c = vertices[t.id[2]];

    t.sz = area(a.x,a.y,a.z, b.x,b.y,b.z, c.x,c.y,c.z);
    }

  for(size_t i=1;i<triangle.size();++i) {
    triangle[i].prefix += triangle[i-1].sz;
    triangle[i].prefix += triangle[i-1].prefix;
    }
  }

Tempest::Vec3 PfxEmitterMesh::randCoord(float rnd) const {
  if(triangle.size()==0)
    return Tempest::Vec3();

  rnd*=triangle.back().prefix;

  auto it = std::lower_bound(triangle.begin(),triangle.end(),rnd,[](const Triangle& t,float v){
    return t.prefix<v;
    });
  if(it!=triangle.begin())
    it--;
  return randCoord(*it,rnd);
  }

Tempest::Vec3 PfxEmitterMesh::randCoord(const PfxEmitterMesh::Triangle& t, float rnd) const {
  rnd = (rnd-t.prefix)/t.sz;


  float    det = float(std::numeric_limits<uint16_t>::max());
  uint32_t k   = uint32_t(rnd*float(std::numeric_limits<uint32_t>::max()));

  float r1 = float((k>>16)&0xFFFF)/det;
  float r2 = float((k    )&0xFFFF)/det;

  float sr1 = std::sqrt(r1);

  auto& a = vertices[t.id[0]];
  auto& b = vertices[t.id[1]];
  auto& c = vertices[t.id[2]];
  return a*(1.f-sr1) + b*(sr1*(1-r2)) + c*(r2*sr1);
  }

float PfxEmitterMesh::area(float x1, float y1, float z1,
                           float x2, float y2, float z2,
                           float x3, float y3, float z3) {
  x1 -= x3;
  y1 -= y3;
  z1 -= z3;

  x2 -= x3;
  y2 -= y3;
  z2 -= z3;

  float x = y1*z2 - z1*y2;
  float y = z1*x2 - x1*z2;
  float z = x1*y2 - y1*x2;

  return std::sqrt(x*x + y*y + z*z); //node omnit 0.5
  }
