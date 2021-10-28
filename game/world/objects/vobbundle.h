#pragma once

#include <memory>

#include "vob.h"

class VobBundle {
  public:
    VobBundle() = default;
    VobBundle(World& owner, std::string_view filename);

    void setObjMatrix(const Tempest::Matrix4x4& obj);

  private:
    std::vector<std::unique_ptr<Vob>> rootVobs;
  };

