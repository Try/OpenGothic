#pragma once

#include <Tempest/Texture2d>
#include <Tempest/StorageBuffer>
#include <vector>

class Bindless {
  public:
    std::vector<const Tempest::Texture2d*>     tex;
    std::vector<const Tempest::StorageBuffer*> vbo;
    std::vector<const Tempest::StorageBuffer*> ibo;
    Tempest::StorageBuffer                     iboOffset;
  };

