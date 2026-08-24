#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <vector>

#include "resources.h"

class Bounds final {
  public:
    Bounds() = default;

    void assign(const Tempest::Vec3& cen, float sizeSz);
    void assign(const Bounds& a, const Bounds& b);
    void assign(const Tempest::Vec3* bbox);
    void assign(const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    void assign(const std::vector<Resources::Vertex>& vbo);
    void assign(const std::vector<Resources::Vertex>& vbo, const std::vector<uint32_t>& ibo, size_t iboOffset, size_t iboLenght);

    Tempest::Vec3 bbox[2];
    float         rConservative = 0;

  private:
    void calcR();
  };

