#include "pfxemittermesh.h"

#include "game/compatibility/phoenix.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/pose.h"

#include <Tempest/Log>

PfxEmitterMesh::PfxEmitterMesh(const PackedMesh& src) {
  vertices.resize(src.vertices.size());
  for(size_t i=0;i<vertices.size();++i) {
    vertices[i].x = src.vertices[i].pos[0];
    vertices[i].y = src.vertices[i].pos[1];
    vertices[i].z = src.vertices[i].pos[2];
    }
  for(size_t i=0;i<src.indices.size();i+=3) {
    Triangle t;
    t.id[0] = src.indices[i+0];
    t.id[1] = src.indices[i+1];
    t.id[2] = src.indices[i+2];
    triangle.push_back(t);
    }

  mkIndex();
  }

PfxEmitterMesh::PfxEmitterMesh(const phoenix::model_mesh& library) {
  for(const auto &i : library.meshes) {
    auto src = phoenix_compat::pack_softskin_mesh(i);

    size_t vert0 = vertices.size();
    vertAnim.resize(vert0 + src.vertices.size());
    for(size_t i=0;i<src.vertices.size();++i) {
      auto& v = src.vertices[i];
      for(size_t r=0; r<4; ++r) {
        vertAnim[vert0+i].v[r].x     = v.LocalPositions[r].x;
        vertAnim[vert0+i].v[r].y     = v.LocalPositions[r].y;
        vertAnim[vert0+i].v[r].z     = v.LocalPositions[r].z;

        vertAnim[vert0+i].id[r]      = v.BoneIndices[r];
        vertAnim[vert0+i].weights[r] = v.Weights[r];
      }
    }

    for(size_t i=0;i<src.indices.size();i+=3) {
      Triangle t;
      t.id[0] = src.indices[i+0];
      t.id[1] = src.indices[i+1];
      t.id[2] = src.indices[i+2];
      triangle.push_back(t);
    }
  }

  mkIndex();
}

Tempest::Vec3 PfxEmitterMesh::randCoord(float rnd, const Pose* pose) const {
  if(triangle.size()==0)
    return Tempest::Vec3();

  rnd*=triangle.back().prefix;

  auto it = std::lower_bound(triangle.begin(),triangle.end(),rnd,[](const Triangle& t,float v){
    return t.prefix<v;
    });
  if(it!=triangle.begin())
    it--;
  return randCoord(*it,rnd,pose);
  }

Tempest::Vec3 PfxEmitterMesh::randCoord(const PfxEmitterMesh::Triangle& t, float rnd,
                                        const Pose* pose) const {
  rnd = (rnd-t.prefix)/t.sz;

  float    det = float(std::numeric_limits<uint16_t>::max());
  uint32_t k   = uint32_t(rnd*float(std::numeric_limits<uint32_t>::max()));

  float r1 = float((k>>16)&0xFFFF)/det;
  float r2 = float((k    )&0xFFFF)/det;

  float sr1 = std::sqrt(r1);

  if(vertices.size()>0) {
    auto& a = vertices[t.id[0]];
    auto& b = vertices[t.id[1]];
    auto& c = vertices[t.id[2]];
    return a*(1.f-sr1) + b*(sr1*(1-r2)) + c*(r2*sr1);
    } else {
    auto  a = animCoord(*pose,t.id[0]);
    auto  b = animCoord(*pose,t.id[1]);
    auto  c = animCoord(*pose,t.id[2]);

    return a*(1.f-sr1) + b*(sr1*(1-r2)) + c*(r2*sr1);
    }
  }

Tempest::Vec3 PfxEmitterMesh::animCoord(const Pose& pose, uint32_t id) const {
  Tempest::Vec3 ret = {};
  auto&         v   = vertAnim[id];
  for(size_t i=0; i<4; ++i) {
    auto&         mat = pose.bone(v.id[i]);
    Tempest::Vec3 a   = v.v[i];
    mat.project(a);
    ret += a*v.weights[i];
    }
  return ret;//*0.1f;
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

  return std::sqrt(x*x + y*y + z*z); //note: omnit 0.5
  }

void PfxEmitterMesh::mkIndex() {
  if(vertices.size()>0) {
    for(auto& t:triangle) {
      auto& a = vertices[t.id[0]];
      auto& b = vertices[t.id[1]];
      auto& c = vertices[t.id[2]];

      t.sz = area(a.x,a.y,a.z, b.x,b.y,b.z, c.x,c.y,c.z);
      }
    } else {
    for(auto& t:triangle) {
      auto& a = vertAnim[t.id[0]].v[0];
      auto& b = vertAnim[t.id[1]].v[0];
      auto& c = vertAnim[t.id[2]].v[0];

      t.sz = area(a.x,a.y,a.z, b.x,b.y,b.z, c.x,c.y,c.z);
      }
    }

  for(size_t i=1;i<triangle.size();++i) {
    triangle[i].prefix += triangle[i-1].sz;
    triangle[i].prefix += triangle[i-1].prefix;
    }
  }
