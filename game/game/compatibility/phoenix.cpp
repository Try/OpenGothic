#include "phoenix.h"

#include <limits>

phoenix::bounding_box phoenix_compat::get_total_aabb(const phoenix::softskin_mesh& msh) {
  phoenix::bounding_box bbox = {{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
                                {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()}};

  {
    auto meshBbox = msh.mesh.bbox;

    bbox.min.x = std::min(bbox.min.x, meshBbox.min.x);
    bbox.min.y = std::min(bbox.min.y, meshBbox.min.y);
    bbox.min.z = std::min(bbox.min.z, meshBbox.min.z);

    bbox.max.x = std::max(bbox.max.x, meshBbox.max.x);
    bbox.max.y = std::max(bbox.max.y, meshBbox.max.y);
    bbox.max.z = std::max(bbox.max.z, meshBbox.max.z);
  }

  for (const auto& bb : msh.bboxes) {
    auto meshBbox = bb.as_bbox();

    bbox.min.x = std::min(bbox.min.x, meshBbox.min.x);
    bbox.min.y = std::min(bbox.min.y, meshBbox.min.y);
    bbox.min.z = std::min(bbox.min.z, meshBbox.min.z);

    bbox.max.x = std::max(bbox.max.x, meshBbox.max.x);
    bbox.max.y = std::max(bbox.max.y, meshBbox.max.y);
    bbox.max.z = std::max(bbox.max.z, meshBbox.max.z);
  }

  return bbox;
}

phoenix_compat::PackedSkeletalMesh phoenix_compat::pack_softskin_mesh(const phoenix::softskin_mesh& self) {
  phoenix_compat::PackedSkeletalMesh mesh {};
  std::vector<phoenix_compat::SkeletalVertex> vertices(self.mesh.positions.size());

  auto bb = get_total_aabb(self);
  mesh.bbox[0] = {bb.min.x, bb.min.y, bb.min.z};
  mesh.bbox[1] = {bb.max.x, bb.max.y, bb.max.z};

  // Extract weights and local positions
  const auto& stream = self.weights;

  for(size_t i=0; i<vertices.size(); ++i) {
    for(size_t j=0; j<stream[i].size(); j++) {
      auto& weight = stream[i][j];

      vertices[i].BoneIndices[j]    = weight.node_index;
      vertices[i].LocalPositions[j] = {weight.position.x, weight.position.y, weight.position.z};
      vertices[i].Weights[j]        = weight.weight;
    }
  }

  size_t vboSize = 0;
  size_t iboSize = 0;
  for(size_t s=0; s<self.mesh.sub_meshes.size(); s++) {
    const auto& sm = self.mesh.sub_meshes[s];
    vboSize += sm.wedges.size();
    iboSize += sm.triangles.size()*3;
  }

  mesh.vertices.resize(vboSize);
  mesh.indices .resize(iboSize);
  mesh.subMeshes.resize(self.mesh.sub_meshes.size());
  auto* vbo = mesh.vertices.data();
  auto* ibo = mesh.indices.data();

  uint32_t meshVxStart = 0, iboStart = 0;
  for(size_t s=0; s<self.mesh.sub_meshes.size(); s++) {
    const auto& sm = self.mesh.sub_meshes[s];
    // Get data
    for(size_t i=0; i<sm.wedges.size(); ++i) {
      const auto&  wedge = sm.wedges[i];
      phoenix_compat::SkeletalVertex v   = vertices[wedge.index];

      v.Normal   = {wedge.normal.x, wedge.normal.y, wedge.normal.z};
      v.TexCoord = {wedge.texture.x, wedge.texture.y};
      v.Color    = 0xFFFFFFFF;  // TODO: Apply color from material!
      *vbo = v;
      ++vbo;
    }

    // And get the indices
    for(size_t i=0; i<sm.triangles.size(); ++i) {
      for(int j=0; j<3; j++) {
        *ibo = sm.triangles[i].wedges[j] + meshVxStart;
        ++ibo;
      }
    }

    auto& pack = mesh.subMeshes[s];
    pack.indexOffset = iboStart;
    pack.indexSize   = sm.triangles.size()*3;
    pack.material    = sm.mat;
    meshVxStart += uint32_t(sm.wedges.size());
    iboStart    += uint32_t(sm.triangles.size()*3);
  }

  return mesh;
}
