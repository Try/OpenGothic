#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"

#include "resources.h"

class PackedMesh;

class ProtoMesh {
  public:
    using Vertex =Resources::VertexA;

    ProtoMesh(PackedMesh&&  pm, const std::string& fname);
    ProtoMesh(PackedMesh&&  pm, const std::vector<ZenLoad::zCMorphMesh::Animation>& aniList, const std::string& fname);
    ProtoMesh(const ZenLoad::zCModelMeshLib& lib, std::unique_ptr<Skeleton>&& sk, const std::string& fname);
    ProtoMesh(const Material& mat, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo); //decals
    ProtoMesh(ProtoMesh&&)=default;
    ProtoMesh& operator=(ProtoMesh&&)=default;
    ~ProtoMesh();

    struct SubMesh final {
      Material                         material;
      Tempest::IndexBuffer<uint32_t>   ibo;
      std::unique_ptr<PhysicMeshShape> shape;
      };

    struct SubMeshId final {
      size_t                id    = 0;
      size_t                subId = 0;
      };

    struct Node {
      size_t                attachId  = size_t(-1);
      size_t                parentId  = size_t(-1);
      bool                  hasChild  = false;
      Tempest::Matrix4x4    transform;

      size_t                submeshIdB = 0;
      size_t                submeshIdE = 0;
      };

    struct Attach : StaticMesh {
      using StaticMesh::StaticMesh;
      Attach(Attach&&)=default;
      Attach& operator=(Attach&&)=default;
      ~Attach();

      std::string                      name;
      bool                             hasNode=false;
      std::unique_ptr<PhysicMeshShape> shape;
      };

    struct Pos {
      std::string        name;
      Tempest::Matrix4x4 transform;
      size_t             node=0;
      };

    struct Animation {
      std::string            name;
      size_t                 numFrames       = 0;
      size_t                 samplesPerFrame = 0;
      int32_t                layer           = 0;
      uint64_t               tickPerFrame    = 50;
      uint64_t               duration        = 0;

      size_t                 index           = 0;
      };

    // animation
    std::unique_ptr<Skeleton>      skeleton;
    std::vector<AnimMesh>          skined;
    std::vector<Animation>         morph;
    Tempest::StorageBuffer         morphIndex;
    Tempest::StorageBuffer         morphSamples;

    std::vector<Attach>            attach;
    std::vector<Node>              nodes;
    std::vector<SubMeshId>         submeshId;

    std::vector<Pos>               pos;
    Tempest::Vec3                  bbox[2];

    std::string                    scheme, fname;

    size_t                         skinedNodesCount() const;
    Tempest::Matrix4x4             mapToRoot(size_t node) const;
    size_t                         findNode(std::string_view name,size_t def=size_t(-1)) const;

  private:
    void                           setupScheme(const std::string& s);
    void                           remap(const ZenLoad::zCMorphMesh::Animation& a,
                                         const std::vector<uint32_t>& vertId,
                                         std::vector<int32_t>& remapId,
                                         std::vector<Tempest::Vec4>& samples,
                                         size_t idOffset);

    Animation                      mkAnimation(const ZenLoad::zCMorphMesh::Animation& a);
  };
