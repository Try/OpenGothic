#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "bounds.h"
#include "material.h"
#include "resources.h"

class World;
class SceneGlobals;
class Light;
class PackedMesh;
class Painter3d;

class Landscape final {
  public:
    Landscape(const SceneGlobals& scene, const PackedMesh& wmesh);

    void setupUbo();
    void draw      (Painter3d& painter, uint8_t frameId);
    void drawShadow(Painter3d& painter, uint8_t frameId, int layer);

  private:
    struct PerFrame {
      std::vector<Tempest::Uniforms>  ubo[3];
      Tempest::Uniforms               solidSh[2];
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

    const SceneGlobals&            scene;
    PerFrame                       perFrame[Resources::MaxFramesInFlight];
  };
