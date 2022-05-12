#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zCMaterial.h>

#include <Tempest/Vec>
#include <unordered_map>
#include <map>
#include <utility>

class Bounds;

class PackedMesh {
  public:
    using WorldTriangle = ZenLoad::WorldTriangle;
    using WorldVertex   = ZenLoad::WorldVertex;

    enum PkgType {
      PK_Visual,
      PK_VisualLnd,
      PK_Physic,
      };

    struct SubMesh final {
      ZenLoad::zCMaterialData material;
      size_t                  iboOffset = 0;
      size_t                  iboLength = 0;
      };

    std::vector<WorldVertex>   vertices;
    std::vector<uint32_t>      indices;
    std::vector<SubMesh>       subMeshes;

    PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type);
    void debug(std::ostream &out) const;

    std::pair<Tempest::Vec3,Tempest::Vec3> bbox() const;

  private:
    enum {
      MaxVert     = 64,
      MaxInd      = 126, // NVidia allocates pipeline memory in batches of 128 bytes (2 reserved for size)
      MaxMeshlets = 16,
      };

    using  Vert = std::pair<uint32_t,uint32_t>;
    struct Meshlet {
      struct Bounds {
        Tempest::Vec3 at;
        float         r = 0;
        };

      Vert    vert   [MaxVert] = {};
      uint8_t indexes[MaxInd]  = {};
      uint8_t vertSz           = 0;
      uint8_t indSz            = 0;
      Bounds  bounds;

      void    flush(std::vector<WorldVertex>& vertices, std::vector<uint32_t>& indices,
                    SubMesh& sub, const ZenLoad::zCMesh& mesh);
      bool    insert(const Vert& a, const Vert& b, const Vert& c, uint8_t matchHint);
      void    clear();
      void    updateBounds(const ZenLoad::zCMesh& mesh);
      bool    canMerge(const Meshlet& other) const;
      bool    hasIntersection(const Meshlet& other) const;
      void    merge(const Meshlet& other);
      };

    void   packMeshlets(const ZenLoad::zCMesh& mesh);
    void   postProcessP1(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet>& meshlets);
    void   postProcessP2(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet*>& meshlets);

    void   packPhysics(const ZenLoad::zCMesh& mesh,PkgType type);
  };

