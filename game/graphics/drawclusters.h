#pragma once

#include <Tempest/StorageBuffer>
#include <Tempest/Vec>
#include <cstdint>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/drawbuckets.h"

class DrawClusters {
  public:
    DrawClusters();
    ~DrawClusters();

    using Bucket = DrawBuckets::Bucket;

    struct Cluster final {
      Tempest::Vec3 pos;
      float         r            = 0;
      uint16_t      commandId    = 0;
      uint16_t      bucketId     = 0;
      uint32_t      firstMeshlet = 0;
      uint32_t      meshletCount = 0;
      uint32_t      instanceId   = 0;
      };

    Cluster& operator[](size_t i) { return clusters[i]; }
    size_t   size() const { return clusters.size(); }
    void     markClusters(size_t id, size_t count = 1);

    uint32_t alloc(const PackedMesh::Cluster* cluster, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);
    uint32_t alloc(const Bucket&  bucket,  size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);
    void     free(uint32_t id, uint32_t numCluster);

    bool     commit(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    auto     ssbo() -> Tempest::StorageBuffer& { return clustersGpu; }
    auto     ssbo() const -> const Tempest::StorageBuffer& { return clustersGpu; }

  private:
    struct Range {
      size_t begin = 0;
      size_t end   = 0;
      };

    struct ScratchPatch {
      std::vector<uint32_t> header;
      std::vector<Cluster>  patch;
      };

    struct Patch {
      Tempest::StorageBuffer indices;
      Tempest::StorageBuffer data;
      };

    void                           patchClusters(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    size_t                         implAlloc(size_t count);

    std::vector<Cluster>           clusters;
    std::vector<Range>             freeList;
    Tempest::StorageBuffer         clustersGpu;
    std::vector<uint32_t>          clustersDurty;
    std::atomic_bool               clustersDurtyBit {false};

    ScratchPatch                   scratch;

    Patch                          patch[Resources::MaxFramesInFlight];
  };
