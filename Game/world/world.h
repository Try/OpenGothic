#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>

#include <daedalus/DaedalusVM.h>
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

    struct Dodad {
      StaticMesh*                    mesh   =nullptr;
      Tempest::Matrix4x4             objMat;
      };

    bool isEmpty() const { return name.empty(); }

    const Tempest::VertexBuffer<Resources::Vertex>& landVbo() const { return vbo; }
    const std::vector<Block>& landBlocks() const { return blocks; }

    std::vector<Dodad> staticObj;

  private:
    std::string                           name;
    Gothic*                               gothic=nullptr;
    std::unique_ptr<Daedalus::DaedalusVM> vm;

    Tempest::VertexBuffer<Resources::Vertex> vbo;

    std::vector<Block>                       blocks;

    void    loadVob(const ZenLoad::zCVobData &vob);
    int32_t runFunction(const std::string &fname,bool clearDataStack);
  };
