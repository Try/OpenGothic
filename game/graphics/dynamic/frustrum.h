#pragma once

#include <Tempest/Matrix4x4>

class Frustrum {
  public:
    void make(const Tempest::Matrix4x4& m, int32_t w, int32_t h);
    void clear();

    bool testPoint(float x, float y, float z) const;
    bool testPoint(float x, float y, float z, float R) const;
    bool testPoint(const Tempest::Vec3& p, float R) const;
    bool testPoint(const Tempest::Vec3& p, float R, float& dist) const;

    enum Ret:uint8_t {
      T_Invisible,
      T_Partial,
      T_Full,
      };
    Ret  testBbox (const Tempest::Vec3& min, const Tempest::Vec3& max) const;

    float              f[6][4] = {};
    Tempest::Matrix4x4 mat;
    uint32_t           width  = 0;
    uint32_t           height = 0;
  };

