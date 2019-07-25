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

    void setMatrix(uint32_t frameId, const Tempest::Matrix4x4& mat, const Tempest::Matrix4x4 *sh, size_t shCount);
    void setLight(const std::array<float,3>& l);

    void commitUbo(uint32_t frameId, const Tempest::Texture2d &shadowMap);
    void draw(Tempest::CommandBuffer &cmd, uint32_t frameId);
    void drawShadow(Tempest::CommandBuffer &cmd, uint32_t frameId, int layer);

  private:
    struct UboLand {
      std::array<float,3> lightDir={{0,0,1}};
      float               padding=0;
      Tempest::Matrix4x4  mvp;
      Tempest::Matrix4x4  shadow;
      };

    struct PerFrame {
      std::vector<Tempest::Uniforms> ubo[3];
      Tempest::UniformBuffer         uboGpu[2];
      };

    struct Block {
      const Tempest::Texture2d*      texture = nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      bool                           alpha=false;
      };

    void implDraw(Tempest::CommandBuffer &cmd, const Tempest::RenderPipeline &p, const Tempest::RenderPipeline &alpha, uint8_t uboId, uint32_t frameId);

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    const RendererStorage&         storage;
    UboLand                        uboCpu;
    std::unique_ptr<PerFrame[]>    pf;
  };
