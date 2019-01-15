#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <string>

#include <zenload/zTypes.h>

#include "resources.h"

class Gothic;

class World final {
  public:
    World()=default;
    World(Gothic &gothic, std::string file);

    struct Block {
      Tempest::Texture2d*            texture=nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    bool isEmpty() const { return name.empty(); }

    const Tempest::VertexBuffer<Resources::LandVertex>& landVbo() const { return vbo; }
    const std::vector<Block>& landBlocks() const { return blocks; }

  private:
    std::string name;
    Gothic*     gothic=nullptr;

    Tempest::VertexBuffer<Resources::LandVertex> vbo;

    std::vector<Block>                           blocks;
  };
