#pragma once

#include <Tempest/AccelerationStructure>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include <vector>

class Material;
class StaticMesh;

class RtScene {
  public:
    RtScene();

    enum Category : uint8_t {
      None,
      Landscape,
      Static,
      Movable,
      };

    struct RtObjectDesc {
      uint32_t instanceId;
      uint32_t firstPrimitive : 24;
      uint32_t bits: 8;
      };

    void notifyTlas(const Material& m, RtScene::Category cat) const;
    bool isUpdateRequired() const;

    void addInstance(const Tempest::Matrix4x4& pos, const Tempest::AccelerationStructure& blas,
                     const Material& mat, const StaticMesh& mesh, size_t firstIndex, size_t iboLength, Category cat);
    void buildTlas();

    Tempest::AccelerationStructure             tlas;
    // Tempest::AccelerationStructure             tlasLand;

    std::vector<const Tempest::Texture2d*>     tex;
    std::vector<const Tempest::StorageBuffer*> vbo;
    std::vector<const Tempest::StorageBuffer*> ibo;
    Tempest::StorageBuffer                     rtDesc;

  private:
    struct BuildBlas {
      std::vector<Tempest::RtGeometry> geom;
      std::vector<RtObjectDesc>        rtDesc;
      };

    struct Build {
      std::vector<const Tempest::Texture2d*>     tex;
      std::vector<const Tempest::StorageBuffer*> vbo;
      std::vector<const Tempest::StorageBuffer*> ibo;

      std::vector<RtObjectDesc>                  rtDesc;
      std::vector<Tempest::RtInstance>           inst;

      BuildBlas                                  staticOpaque;
      BuildBlas                                  staticAt;
      };

    uint32_t aquireBucketId(const Material& mat, const StaticMesh& mesh);
    void     addInstance(const BuildBlas& build, Tempest::AccelerationStructure& blas, Tempest::RtInstanceFlags flags);

    Build                          build;
    Tempest::AccelerationStructure blasStaticOpaque;
    Tempest::AccelerationStructure blasStaticAt;

    mutable bool                   needToUpdate = true;
  };

