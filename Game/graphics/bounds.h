#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <zenload/zCMesh.h>

#include <vector>

#include "resources.h"

class Bounds final {
  public:
    Bounds();

    void assign(const ZMath::float3* bbox);
    void assign(const std::vector<Resources::Vertex>& vbo);
    void setObjMatrix(const Tempest::Matrix4x4& m);

    Tempest::Vec3 bbox[2];
    Tempest::Vec3 bboxTr[2];
    Tempest::Vec3 at;
    float         r = 0;

  private:
    void transformBbox(const Tempest::Matrix4x4& m);
    void calcR();
  };

