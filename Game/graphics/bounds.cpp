#include "bounds.h"

using namespace Tempest;

Bounds::Bounds(){
  }

void Bounds::assign(const ZMath::float3* src) {
  std::memcpy(bbox,   src, 2*sizeof(Vec3));
  std::memcpy(bboxTr, src, 2*sizeof(Vec3));
  calcR();
  }

void Bounds::assign(const std::vector<Resources::Vertex>& vbo) {
  if(vbo.size()==0){
    std::memset(bbox,   0, 2*sizeof(Vec3));
    std::memset(bboxTr, 0, 2*sizeof(Vec3));
    r = 0;
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
  bboxTr[0] = bbox[0];
  bboxTr[1] = bbox[1];
  calcR();
  }

void Bounds::assign(const std::vector<ZenLoad::WorldVertex>& vbo, const std::vector<uint32_t>& ibo) {
  if(ibo.size()==0){
    std::memset(bbox,   0, 2*sizeof(Vec3));
    std::memset(bboxTr, 0, 2*sizeof(Vec3));
    r = 0;
    return;
    }

  bbox[0].x = vbo[ibo[0]].Position.x;
  bbox[0].y = vbo[ibo[0]].Position.y;
  bbox[0].z = vbo[ibo[0]].Position.z;
  bbox[1] = bbox[0];
  for(auto id:ibo) {
    auto& i = vbo[id];
    bbox[0].x = std::min(bbox[0].x,i.Position.x);
    bbox[0].y = std::min(bbox[0].y,i.Position.y);
    bbox[0].z = std::min(bbox[0].z,i.Position.z);
    bbox[1].x = std::max(bbox[1].x,i.Position.x);
    bbox[1].y = std::max(bbox[1].y,i.Position.y);
    bbox[1].z = std::max(bbox[1].z,i.Position.z);
    }
  bboxTr[0] = bbox[0];
  bboxTr[1] = bbox[1];
  calcR();
  }

void Bounds::setObjMatrix(const Matrix4x4& m) {
  at = Vec3(m.at(3,0),m.at(3,1),m.at(3,2));
  transformBbox(m);
  calcR();
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

  for(auto& i:pt){
    m.project(i.x,i.y,i.z);
    }
  bboxTr[0].x = pt[0].x;
  bboxTr[0].y = pt[0].y;
  bboxTr[0].z = pt[0].z;
  bboxTr[1] = bboxTr[0];
  for(auto& i:pt) {
    bboxTr[0].x = std::min(bboxTr[0].x,i.x);
    bboxTr[0].y = std::min(bboxTr[0].y,i.y);
    bboxTr[0].z = std::min(bboxTr[0].z,i.z);
    bboxTr[1].x = std::max(bboxTr[1].x,i.x);
    bboxTr[1].y = std::max(bboxTr[1].y,i.y);
    bboxTr[1].z = std::max(bboxTr[1].z,i.z);
    }
  }

void Bounds::calcR() {
  float x0 = std::fabs(bboxTr[0].x-at.x);
  float y0 = std::fabs(bboxTr[0].y-at.y);
  float z0 = std::fabs(bboxTr[0].z-at.z);
  float x1 = std::fabs(bboxTr[1].x-at.x);
  float y1 = std::fabs(bboxTr[1].y-at.y);
  float z1 = std::fabs(bboxTr[1].z-at.z);

  float x = std::max(x1,x0),
        y = std::max(y1,y0),
        z = std::max(z1,z0);
  r = std::sqrt(x*x+y*y+z*z);
  }
