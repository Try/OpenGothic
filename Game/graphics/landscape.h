#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "resources.h"

class World;
class RendererStorage;

class Landscape final {
  public:
    Landscape(const RendererStorage& storage,const ZenLoad::PackedMesh& wmesh);

    void setMatrix(uint32_t frameId,const Tempest::Matrix4x4& mat);
    void commitUbo(uint32_t frameId);
    void draw(Tempest::CommandBuffer &cmd, uint32_t frameId);

  private:
    struct UboLand {
      Tempest::Matrix4x4 mvp;
      };

    struct PerFrame {
      std::vector<Tempest::Uniforms> uboLand;
      Tempest::UniformBuffer         uboGpu;
      };

    struct Block {
      const Tempest::Texture2d*      texture = nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      bool                           alpha=false;
      };

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    const RendererStorage&         storage;
    UboLand                        uboCpu;
    std::unique_ptr<PerFrame[]>    pf;
  };
