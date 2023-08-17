#pragma once

#include <Tempest/AccelerationStructure>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include <vector>

class RtScene {
  public:
    RtScene();

    Tempest::AccelerationStructure             tlas;
    // Tempest::AccelerationStructure             tlasLand;

    std::vector<const Tempest::Texture2d*>     tex;
    std::vector<const Tempest::StorageBuffer*> vbo;
    std::vector<const Tempest::StorageBuffer*> ibo;
    Tempest::StorageBuffer                     iboOffset;
  };

