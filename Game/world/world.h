#pragma once

#include <Tempest/VertexBuffer>
#include <string>

#include <zenload/zTypes.h>

#include "resources.h"

class Gothic;

class World final {
  public:
    World()=default;
    World(Gothic &gothic, std::string file);

    bool isEmpty() const { return name.empty(); }

    const Tempest::VertexBuffer<Resources::LandVertex>& landVbo() const { return vbo; }

  private:
    std::string name;
    Gothic*     gothic=nullptr;

    Tempest::VertexBuffer<Resources::LandVertex> vbo;
  };
