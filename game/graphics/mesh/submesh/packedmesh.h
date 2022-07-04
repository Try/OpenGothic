#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zCMaterial.h>
#include <zenload/zCProgMeshProto.h>
#include <zenload/zCMeshSoftSkin.h>

#include <Tempest/Vec>
#include <unordered_map>
#include <map>
#include <utility>

#include "resources.h"

class Bounds;

class PackedMesh {
  public:
    using WorldTriangle = ZenLoad::WorldTriangle;
    using Vertex        = Resources::Vertex;
    using VertexA       = Resources::VertexA;

    enum {
      MaxVert     = 64,
      // NVidia allocates pipeline memory in batches of 128 bytes (4 reserved for size)
      MaxInd      = 41*3,
      MaxMeshlets = 16,
      };

    enum PkgType {
      PK_Visual,
      PK_VisualLnd,
      PK_VisualMorph,
      PK_Physic,
      };

    struct SubMesh final {
      ZenLoad::zCMaterialData material;
      size_t                  iboOffset = 0;
      size_t                  iboLength = 0;
      };

    struct Bounds final {
      Tempest::Vec3 pos;
      float         r = 0;
      };

    std::vector<Vertex>   vertices;
    std::vector<VertexA>  verticesA;
    std::vector<uint32_t> indices;
    std::vector<SubMesh>  subMeshes;
    std::vector<Bounds>   meshletBounds;

    std::vector<uint32_t>    verticesId; // only for morph meshes
    bool                     isUsingAlphaTest = true;

    PackedMesh(const ZenLoad::zCMesh&          mesh, PkgType type);
    PackedMesh(const ZenLoad::zCProgMeshProto& mesh, PkgType type);
    PackedMesh(const ZenLoad::zCMeshSoftSkin&  mesh);
    void debug(std::ostream &out) const;

    std::pair<Tempest::Vec3,Tempest::Vec3> bbox() const;

  private:
    size_t        maxIboSliceLength = 0;
    float         clusterRadius     = 20*100;
    Tempest::Vec3 mBbox[2];

    struct SkeletalData {
      ZMath::float3 localPositions[4] = {};
      uint8_t       boneIndices[4]    = {};
      float         weights[4]        = {};
      };

    using  Vert = std::pair<uint32_t,uint32_t>;
    struct Meshlet {
      Vert          vert   [MaxVert] = {};
      uint8_t       indexes[MaxInd]  = {};
      uint8_t       vertSz           = 0;
      uint8_t       indSz            = 0;
      Bounds        bounds;

      void    flush(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<Bounds>& instances,
                    SubMesh& sub, const ZenLoad::zCMesh& mesh);
      void    flush(std::vector<Vertex>& vertices, std::vector<VertexA>& verticesA,
                    std::vector<uint32_t>& indices, std::vector<uint32_t>* verticesId,
                    SubMesh& sub, const std::vector<ZMath::float3>& vbo,
                    const std::vector<ZenLoad::zWedge>& wedgeList,
                    const std::vector<SkeletalData>* skeletal);

      bool    insert(const Vert& a, const Vert& b, const Vert& c, uint8_t matchHint);
      void    clear();
      void    updateBounds(const ZenLoad::zCMesh& mesh);
      void    updateBounds(const ZenLoad::zCProgMeshProto& mesh);
      void    updateBounds(const std::vector<ZMath::float3>& vbo);
      bool    canMerge(const Meshlet& other) const;
      bool    hasIntersection(const Meshlet& other) const;
      float   qDistance(const Meshlet& other) const;
      void    merge(const Meshlet& other);
      };

    void   addIndex(Meshlet* active, size_t numActive, std::vector<Meshlet>& meshlets,
                    const Vert& a, const Vert& b, const Vert& c);
    void   packMeshlets(const ZenLoad::zCMesh& mesh);
    void   packMeshlets(const ZenLoad::zCProgMeshProto& mesh, PkgType type,
                        const std::vector<SkeletalData>* skeletal);

    void   postProcessP1(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet>& meshlets);
    void   postProcessP2(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet*>& meshlets);

    void   sortPass(std::vector<Meshlet*>& meshlets);
    void   mergePass(std::vector<Meshlet*>& meshlets, bool fast);

    void   packPhysics(const ZenLoad::zCMesh& mesh,PkgType type);
    void   computeBbox();

    void   dbgUtilization(std::vector<Meshlet*>& meshlets);
  };

