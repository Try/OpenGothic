#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <zenload/zCMesh.h>

#include <vector>

#include "resources.h"

class Bounds final {
  public:
    Bounds();

    void assign(const Tempest::Vec3& cen, float sizeSz);
    void assign(const Bounds& a, const Bounds& b);
    void assign(const ZMath::float3* bbox);
    void assign(const std::vector<Resources::Vertex>& vbo);
    void assign(const std::vector<ZenLoad::WorldVertex>& vbo,const std::vector<uint32_t>& ibo);
    void setObjMatrix(const Tempest::Matrix4x4& m);

    Tempest::Vec3 bbox[2];
    Tempest::Vec3 bboxTr[2];
    Tempest::Vec3 at;
    Tempest::Vec3 midTr;
    float         r = 0;

  private:
    void transformBbox(const Tempest::Matrix4x4& m);
    void calcR();
  };

