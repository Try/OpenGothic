#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "bounds.h"
#include "material.h"
#include "resources.h"

class World;
class RendererStorage;
class Light;
class PackedMesh;
class Painter3d;

class Landscape final {
  public:
    Landscape(const RendererStorage& storage,const PackedMesh& wmesh);

    void setMatrix(uint32_t frameId, const Tempest::Matrix4x4& mat, const Tempest::Matrix4x4 *sh, size_t shCount);
    void setLight (const Light& l, const Tempest::Vec3& ambient);

    void invalidateCmd();
    bool needToUpdateCommands(uint8_t frameId) const;
    void setAsUpdated        (uint8_t frameId);

    void commitUbo (uint8_t frameId, const Tempest::Texture2d& shadowMap);
    void draw      (Painter3d& painter, uint8_t frameId);
    void drawShadow(Painter3d& painter, uint8_t frameId, int layer);

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
      Tempest::Uniforms               solidSh[2];
      bool                            nToUpdate=true;
      };

    struct Block {
      Material                       material;
      Bounds                         bbox;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    void implDraw(Painter3d& painter,
                  const Tempest::RenderPipeline* p[],
                  uint8_t uboId, uint8_t frameId);

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    Tempest::IndexBuffer<uint32_t>           iboSolid;
    Bounds                                   solidsBBox;

    const RendererStorage&         storage;
    UboLand                        uboCpu;
    PerFrame                       pf[Resources::MaxFramesInFlight];
  };
