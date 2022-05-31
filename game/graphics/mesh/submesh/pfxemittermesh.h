#pragma once

#include <cstdint>

#include <phoenix/model_mesh.hh>

#include <Tempest/Point>

class PackedMesh;
class Pose;

class PfxEmitterMesh {
  public:
    PfxEmitterMesh(const PackedMesh& src);
    PfxEmitterMesh(const phoenix::model_mesh& src);

    Tempest::Vec3 randCoord(float rnd, const Pose* pose) const;

  private:
    struct Triangle {
      uint32_t id[3]  = {};
      float    sz     = 0;
      float    prefix = 0;
      };

    struct AnimData {
      Tempest::Vec3 v[4];
      uint8_t       id[4] = {};
      float         weights[4] = {};
      };

    Tempest::Vec3 randCoord(const Triangle& t, float rnd, const Pose* pose) const;
    Tempest::Vec3 animCoord(const Pose& pose, uint32_t index) const;
    void          mkIndex();

    static float area(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3);

    std::vector<Triangle>      triangle;
    std::vector<Tempest::Vec3> vertices;
    std::vector<AnimData>      vertAnim;
  };

