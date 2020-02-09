#pragma once

#include <zenload/zCMesh.h>
#include <unordered_map>
#include <map>

class PackedMesh {
  public:
    using WorldTriangle = ZenLoad::WorldTriangle;
    using WorldVertex   = ZenLoad::WorldVertex;

    enum PkgType {
      PK_Visual,
      PK_Physic
      };

    struct SubMesh final {
      ZenLoad::zCMaterialData material;
      std::vector<uint32_t>   indices;
      };

    std::vector<WorldVertex>   vertices;
    std::vector<SubMesh>       subMeshes;
    ZMath::float3              bbox[2] = {};

    PackedMesh(ZenLoad::zCMesh& mesh,PkgType type);

  private:
    void   pack(const ZenLoad::zCMesh& mesh,PkgType type);
    size_t submeshIndex(const ZenLoad::zCMesh& mesh,size_t vertexId,PkgType type) const;
  };

