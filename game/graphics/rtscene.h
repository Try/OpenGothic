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

    void notifyTlas(const Material& m, RtScene::Category cat) const;
    bool isUpdateRequired() const;

    void addInstance(const Tempest::Matrix4x4& pos, const Tempest::AccelerationStructure& blas,
                     const Material& mat, const StaticMesh& mesh, size_t firstIndex, size_t iboLength, Category cat);
    void addInstance(const Tempest::AccelerationStructure& blas);
    void buildTlas();

    Tempest::AccelerationStructure             tlas;
    // Tempest::AccelerationStructure             tlasLand;

    std::vector<const Tempest::Texture2d*>     tex;
    std::vector<const Tempest::StorageBuffer*> vbo;
    std::vector<const Tempest::StorageBuffer*> ibo;
    Tempest::StorageBuffer                     iboOffset;

  private:
    struct Build {
      std::vector<const Tempest::Texture2d*>     tex;
      std::vector<const Tempest::StorageBuffer*> vbo;
      std::vector<const Tempest::StorageBuffer*> ibo;
      std::vector<uint32_t>                      iboOff;
      std::vector<Tempest::RtInstance>           inst;

      std::vector<Tempest::RtGeometry>           staticOpaque;
      } build;

    Tempest::AccelerationStructure blasStaticOpaque;
    mutable bool                   needToUpdate = true;
  };

