#include "bounds.h"

#include <Tempest/Log>

using namespace Tempest;

Bounds::Bounds(){
  }

void Bounds::assign(const Vec3& cen, float sizeSz) {
  bbox[0] = cen-Vec3(sizeSz,sizeSz,sizeSz);
  bbox[1] = cen+Vec3(sizeSz,sizeSz,sizeSz);
  midTr   = cen;
  mid     = cen;
  calcR();
  }

void Bounds::assign(const Bounds& a, const Bounds& b) {
  bbox[0].x = std::min(a.bbox[0].x,b.bbox[0].x);
  bbox[0].y = std::min(a.bbox[0].y,b.bbox[0].y);
  bbox[0].z = std::min(a.bbox[0].z,b.bbox[0].z);
  bbox[1].x = std::max(a.bbox[1].x,b.bbox[1].x);
  bbox[1].y = std::max(a.bbox[1].y,b.bbox[1].y);
  bbox[1].z = std::max(a.bbox[1].z,b.bbox[1].z);
  mid       = (bbox[0]+bbox[1])/2;
  midTr     = mid;
  calcR();
  }

void Bounds::assign(const Vec3* src) {
  bbox[0] = src[0];
  bbox[1] = src[1];
  mid     = (bbox[0]+bbox[1])/2;
  midTr   = mid;
  calcR();
  }

void Bounds::assign(const std::pair<Tempest::Vec3, Tempest::Vec3>& src) {
  bbox[0] = src.first;
  bbox[1] = src.second;
  mid     = (bbox[0]+bbox[1])/2;
  midTr   = mid;
  calcR();
  }

void Bounds::assign(const std::vector<Resources::Vertex>& vbo) {
  if(vbo.size()==0) {
    bbox[0] = Vec3();
    bbox[1] = Vec3();
    mid     = Vec3();
    midTr   = Vec3();
    r       = 0;
    return;
    }
  bbox[0].x = vbo[0].pos[0];
  bbox[0].y = vbo[0].pos[1];
  bbox[0].z = vbo[0].pos[2];
  bbox[1] = bbox[0];
  for(auto& i:vbo) {
    bbox[0].x = std::min(bbox[0].x,i.pos[0]);
    bbox[0].y = std::min(bbox[0].y,i.pos[1]);
    bbox[0].z = std::min(bbox[0].z,i.pos[2]);
    bbox[1].x = std::max(bbox[1].x,i.pos[0]);
    bbox[1].y = std::max(bbox[1].y,i.pos[1]);
    bbox[1].z = std::max(bbox[1].z,i.pos[2]);
    }
  mid   = (bbox[0]+bbox[1])/2;
  midTr = mid;
  calcR();
  }

void Bounds::assign(const std::vector<Resources::Vertex>& vbo, const std::vector<uint32_t>& ibo,
                    size_t iboOffset, size_t iboLenght) {
  if(ibo.size()==0){
    bbox[0] = Vec3();
    bbox[1] = Vec3();
    mid     = Vec3();
    midTr   = Vec3();
    r       = 0;
    return;
    }

  bbox[0].x = vbo[ibo[0]].pos[0];
  bbox[0].y = vbo[ibo[0]].pos[1];
  bbox[0].z = vbo[ibo[0]].pos[2];
  bbox[1] = bbox[0];
  for(size_t id=0; id<iboLenght; ++id) {
    auto& i = vbo[ibo[iboOffset+id]];
    bbox[0].x = std::min(bbox[0].x,i.pos[0]);
    bbox[0].y = std::min(bbox[0].y,i.pos[1]);
    bbox[0].z = std::min(bbox[0].z,i.pos[2]);
    bbox[1].x = std::max(bbox[1].x,i.pos[0]);
    bbox[1].y = std::max(bbox[1].y,i.pos[1]);
    bbox[1].z = std::max(bbox[1].z,i.pos[2]);
    }
  mid   = (bbox[0]+bbox[1])/2;
  midTr = mid;
  calcR();
  }

void Bounds::setObjMatrix(const Matrix4x4& m) {
  // transformBbox(m);
  midTr = mid;
  m.project(midTr);
  }

void Bounds::transformBbox(const Matrix4x4& m) {
  auto* b = bbox;
  Vec3 pt[8] = {
    {b[0].x,b[0].y,b[0].z},
    {b[1].x,b[0].y,b[0].z},
    {b[1].x,b[1].y,b[0].z},
    {b[0].x,b[1].y,b[0].z},

    {b[0].x,b[0].y,b[1].z},
    {b[1].x,b[0].y,b[1].z},
    {b[1].x,b[1].y,b[1].z},
    {b[0].x,b[1].y,b[1].z},
    };

  for(auto& i:pt)
    m.project(i.x,i.y,i.z);

  midTr = mid;
  m.project(midTr);
  }

void Bounds::calcR() {
  float dx = std::fabs(bbox[0].x-bbox[1].x);
  float dy = std::fabs(bbox[0].y-bbox[1].y);
  float dz = std::fabs(bbox[0].z-bbox[1].z);
  r = std::sqrt(dx*dx+dy*dy+dz*dz)*0.5f;

  float x = std::max(std::abs(bbox[0].x),std::abs(bbox[1].x));
  float y = std::max(std::abs(bbox[0].y),std::abs(bbox[1].y));
  float z = std::max(std::abs(bbox[0].z),std::abs(bbox[1].z));
  rConservative = std::sqrt(x*x+y*y+z*z);
  }
