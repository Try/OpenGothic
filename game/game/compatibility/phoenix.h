#pragma once

#include <phoenix/material.hh>
#include <phoenix/softskin_mesh.hh>

#include <zenload/zTypes.h>

namespace phoenix_compat {
  struct SkeletalVertex
  {
    ZMath::float3 Normal;
    ZMath::float2 TexCoord;
    uint32_t Color;
    ZMath::float3 LocalPositions[4];
    unsigned char BoneIndices[4];
    float Weights[4];
  };

  struct PackedSkeletalMesh
  {
    struct SubMesh
    {
      phoenix::material material;
      size_t           indexOffset = 0;
      size_t           indexSize   = 0;
    };

    ZMath::float3               bbox[2];
    std::vector<SkeletalVertex> vertices;
    std::vector<uint32_t>       indices;
    std::vector<SubMesh>        subMeshes;
  };

  phoenix::bounding_box get_total_aabb(const phoenix::softskin_mesh&);

  PackedSkeletalMesh pack_softskin_mesh(const phoenix::softskin_mesh&);
}
