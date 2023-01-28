#pragma once

#include <phoenix/mesh.hh>
#include <phoenix/proto_mesh.hh>
#include <phoenix/softskin_mesh.hh>
#include <phoenix/material.hh>

#include <Tempest/Vec>
#include <unordered_map>
#include <map>
#include <utility>

#include "resources.h"

class Bounds;

class PackedMesh {
  public:
    using Vertex        = Resources::Vertex;
    using VertexA       = Resources::VertexA;

    enum {
      MaxVert     = 64,
      MaxPrim     = 64,
      MaxInd      = MaxPrim * 3,
      MaxMeshlets = 16,
      };

    enum PkgType {
      PK_Visual,
      PK_VisualLnd,
      PK_VisualMorph,
      PK_Physic,
      };

    struct SubMesh final {
      phoenix::material material;
      size_t            iboOffset = 0;
      size_t            iboLength = 0;
      };

    struct Bounds final {
      Tempest::Vec3 pos;
      float         r = 0;
      };

    std::vector<Vertex>   vertices;
    std::vector<VertexA>  verticesA;
    std::vector<uint32_t> indices;
    std::vector<uint8_t>  indices8;

    std::vector<SubMesh>  subMeshes;
    std::vector<Bounds>   meshletBounds;

    std::vector<uint32_t>    verticesId; // only for morph meshes
    bool                     isUsingAlphaTest = true;

    PackedMesh(const phoenix::proto_mesh& mesh, PkgType type);
    PackedMesh(const phoenix::mesh& mesh, PkgType type);
    PackedMesh(const phoenix::softskin_mesh&  mesh);

    void debug(std::ostream &out) const;

    std::pair<Tempest::Vec3,Tempest::Vec3> bbox() const;

  private:
    Tempest::Vec3 mBbox[2];

    struct SkeletalData {
      Tempest::Vec3 localPositions[4] = {};
      uint8_t       boneIndices[4]    = {};
      float         weights[4]        = {};
      };

    using  Vert = std::pair<uint32_t,uint32_t>;
    struct PrimitiveHeap;
    struct Meshlet {
      Vert          vert   [MaxVert] = {};
      uint8_t       indexes[MaxInd ] = {};
      uint8_t       vertSz           = 0;
      uint8_t       indSz            = 0;
      Bounds        bounds;

      void    flush(std::vector<Vertex>& vertices,
                    std::vector<uint32_t>& indices, std::vector<uint8_t>& indices8,
                    std::vector<Bounds>& instances, const phoenix::mesh& mesh);

      void    flush(std::vector<Vertex>& vertices, std::vector<VertexA>& verticesA,
                    std::vector<uint32_t>& indices, std::vector<uint8_t>& indices8,
                    std::vector<uint32_t>* verticesId, const std::vector<glm::vec3>& vbo,
                    const std::vector<phoenix::wedge>& wedgeList,
                    const std::vector<SkeletalData>* skeletal);

      bool    insert(const Vert& a, const Vert& b, const Vert& c);
      void    clear();
      void    updateBounds(const phoenix::mesh& mesh);
      void    updateBounds(const phoenix::proto_mesh& mesh);
      void    updateBounds(const std::vector<glm::vec3>& vbo);
      bool    canMerge(const Meshlet& other) const;
      bool    hasIntersection(const Meshlet& other) const;
      float   qDistance(const Meshlet& other) const;
      void    merge(const Meshlet& other);
      };

    bool   addTriangle(Meshlet& dest, const phoenix::mesh* mesh, const phoenix::sub_mesh* proto_mesh, size_t id);

    void   packPhysics(const phoenix::mesh& mesh,PkgType type);
    void   packMeshletsLnd(const phoenix::mesh& mesh);
    void   packMeshletsObj(const phoenix::proto_mesh& mesh, PkgType type,
                           const std::vector<SkeletalData>* skeletal);

    std::vector<Meshlet> buildMeshlets(const phoenix::mesh* mesh, const phoenix::sub_mesh* proto_mesh,
                                       PrimitiveHeap& heap, std::vector<bool>& used);

    void   computeBbox();

    void   dbgUtilization(const std::vector<Meshlet>& meshlets);
    void   dbgMeshlets(const phoenix::mesh& mesh, const std::vector<Meshlet*>& meshlets);
  };

