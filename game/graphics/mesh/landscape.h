#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/meshobjects.h"

class World;
class SceneGlobals;
class LightSource;
class PackedMesh;
class WorldView;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

    void prepareUniforms(const SceneGlobals& sc);
    void visibilityPass(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawGBuffer   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);

  private:
    using Item = ObjectsBucket::Item;

    const Tempest::RenderPipeline* pipeline(const Material& m);
    uint32_t                       bucketId(const Material& m);

    struct Block {
      Item                           mesh;
      Tempest::AccelerationStructure blas;
      };

    struct IndirectCmd {
      uint32_t vertexCount   = 0;
      uint32_t instanceCount = 0;
      uint32_t firstVertex   = 0;
      uint32_t firstInstance = 0;
      uint32_t writeOffset   = 0;
      };

    struct BlockCmd {
      const Tempest::RenderPipeline* pso         = nullptr;
      const Tempest::Texture2d*      tex         = nullptr;
      Tempest::DescriptorSet         desc;
      Material::AlphaFunc            alpha       = Material::Solid;
      uint32_t                       maxClusters = 0;
      };

    std::vector<Block>     blocks;
    StaticMesh             mesh;
    Tempest::StorageBuffer meshletDesc;

    Block                  solidBlock;
    Tempest::StorageBuffer meshletSolidDesc;

    uint32_t               clusterTotal = 0;
    Tempest::DescriptorSet descInit, descTask;

    Tempest::StorageBuffer visClusters, indirectCmd;
    Tempest::DescriptorSet desc, descClr, descRes;
    std::vector<BlockCmd>  cmd;
  };
