#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "material.h"
#include "resources.h"

class World;
class RendererStorage;
class Light;
class PackedMesh;

class Landscape final {
  public:
    Landscape(const RendererStorage& storage,const PackedMesh& wmesh);

    void setMatrix(uint32_t frameId, const Tempest::Matrix4x4& mat, const Tempest::Matrix4x4 *sh, size_t shCount);
    void setLight (const Light& l, const Tempest::Vec3& ambient);

    void commitUbo (uint32_t frameId, const Tempest::Texture2d& shadowMap);
    void draw      (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t frameId);
    void drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t frameId, int layer);

  private:
    struct UboLand {
      std::array<float,3> lightDir={{0,0,1}};
      float               padding=0;
      Tempest::Matrix4x4  mvp;
      Tempest::Matrix4x4  shadow;
      std::array<float,4> lightAmb={{0,0,0}};
      std::array<float,4> lightCl ={{1,1,1}};
      };

    struct PerFrame {
      std::vector<Tempest::Uniforms>  ubo[3];
      Tempest::UniformBuffer<UboLand> uboGpu[2];
      };

    struct Block {
      Material                       material;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    void implDraw(Tempest::Encoder<Tempest::CommandBuffer> &cmd,
                  const Tempest::RenderPipeline* p[], uint8_t uboId, uint32_t frameId);

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    const RendererStorage&         storage;
    UboLand                        uboCpu;
    std::unique_ptr<PerFrame[]>    pf;
  };
