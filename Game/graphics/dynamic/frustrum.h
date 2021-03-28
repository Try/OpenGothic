#pragma once

#include <Tempest/Matrix4x4>

class Frustrum {
  public:
    void make(const Tempest::Matrix4x4& m);
    void clear();

    bool testPoint(float x, float y, float z) const;
    bool testPoint(float x, float y, float z, float R) const;
    bool testPoint(const Tempest::Vec3 p, float R) const;

    float f[6][4] = {};
  };

