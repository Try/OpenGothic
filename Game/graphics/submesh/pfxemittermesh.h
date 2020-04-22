#pragma once

#include <zenload/zTypes.h>
#include <cstdint>

#include <Tempest/Point>

class PfxEmitterMesh {
  public:
    PfxEmitterMesh(const ZenLoad::PackedMesh& src);

    Tempest::Vec3 randCoord(float rnd) const;

  private:
    struct Triangle {
      uint32_t id[3]  = {};
      float    sz     = 0;
      float    prefix = 0;
      };

    Tempest::Vec3 randCoord(const Triangle& t, float rnd) const;
    static float area(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3);

    std::vector<Triangle>      triangle;
    std::vector<Tempest::Vec3> vertices;
  };

