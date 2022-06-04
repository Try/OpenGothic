#pragma once

#include <Tempest/Vec>

#include <phoenix/material.hh>
#include <phoenix/softskin_mesh.hh>

namespace phoenix_compat {
  struct SkeletalVertex
  {
    Tempest::Vec3 Normal;
    Tempest::Vec2 TexCoord;
    uint32_t Color;
    Tempest::Vec3 LocalPositions[4];
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

    Tempest::Vec3               bbox[2];
    std::vector<SkeletalVertex> vertices;
    std::vector<uint32_t>       indices;
    std::vector<SubMesh>        subMeshes;
  };

  phoenix::bounding_box get_total_aabb(const phoenix::softskin_mesh&);

  PackedSkeletalMesh pack_softskin_mesh(const phoenix::softskin_mesh&);
}
