#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zCMaterial.h>

#include <unordered_map>
#include <map>

class Bounds;

class PackedMesh {
  public:
    using WorldTriangle = ZenLoad::WorldTriangle;
    using WorldVertex   = ZenLoad::WorldVertex;

    enum PkgType {
      PK_Visual,
      PK_VisualLnd,
      PK_Physic,
      PK_PhysicZoned
      };

    struct SubMesh final {
      ZenLoad::zCMaterialData material;
      std::vector<uint32_t>   indices;
      };

    std::vector<WorldVertex>   vertices;
    std::vector<SubMesh>       subMeshes;
    ZMath::float3              bbox[2] = {};

    PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type);

  private:
    void   pack(const ZenLoad::zCMesh& mesh,PkgType type);

    size_t submeshIndex(const ZenLoad::zCMesh& mesh, std::vector<SubMesh*>& index,
                        size_t vindex, size_t mat, PkgType type);

    void   addSector(const std::string& s, uint8_t group);
    static bool compare(const ZenLoad::zCMaterialData& l, const ZenLoad::zCMaterialData& r);

    void   landRepack();
    void   split(std::vector<SubMesh>& out, SubMesh& src);
  };

