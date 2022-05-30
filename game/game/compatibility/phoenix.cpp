#include "phoenix.h"

ZenLoad::zCMaterialData phoenix_compat::to_zenlib_material(const phoenix::material& px) {
  ZenLoad::zCMaterialData mat {};
  mat.matName = px.name;
  mat.matGroup = uint8_t(px.group);
  mat.color = px.color.r << 24 | px.color.g << 16 | px.color.b << 8 | px.color.a;
  mat.smoothAngle = px.smooth_angle;
  mat.texture = px.texture;
  mat.texScale = std::to_string(px.texture_scale.x) + " " + std::to_string(px.texture_scale.y);
  mat.texAniFPS = px.texture_anim_fps;
  mat.texAniMapMode = px.texture_anim_map_mode;
  mat.texAniMapDir = std::to_string(px.texture_anim_map_dir.x) + " " + std::to_string(px.texture_anim_map_dir.y);
  mat.noCollDet = px.disable_collision;
  mat.noLighmap = px.disable_lightmap;
  mat.loadDontCollapse = px.dont_collapse;
  mat.detailObject = px.detail_object;
  mat.detailTextureScale = px.texture_scale.x;
  mat.forceOccluder = px.force_occluder;
  mat.environmentMapping = px.environment_mapping;
  mat.environmentalMappingStrength = px.environment_mapping_strength;
  mat.waveMode = px.wave_mode;
  mat.waveSpeed = px.wave_speed;
  mat.waveMaxAmplitude = px.wave_max_amplitude;
  mat.waveGridSize = px.wave_grid_size;
  mat.ignoreSun = px.ignore_sun;

  switch(px.alpha_func) {
  case phoenix::alpha_function::test:
    mat.alphaFunc = 1;
    break;
  case phoenix::alpha_function::additive:
    mat.alphaFunc = 5;
    break;
  case phoenix::alpha_function::transparent:
    mat.alphaFunc = 4;
    break;
  case phoenix::alpha_function::multiply:
    mat.alphaFunc = 6;
    break;
  }

  return mat;
}

phoenix::bounding_box phoenix_compat::get_total_aabb(const phoenix::softskin_mesh& msh) {
  phoenix::bounding_box bbox = {{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};

  {
    auto meshBbox = msh.mesh().bbox();

    bbox.min.x = std::min(bbox.min.x, meshBbox.min.x);
    bbox.min.y = std::min(bbox.min.y, meshBbox.min.y);
    bbox.min.z = std::min(bbox.min.z, meshBbox.min.z);

    bbox.max.x = std::max(bbox.max.x, meshBbox.max.x);
    bbox.max.y = std::max(bbox.max.y, meshBbox.max.y);
    bbox.max.z = std::max(bbox.max.z, meshBbox.max.z);
  }

  for (const auto& bb : msh.bboxes()) {
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

ZenLoad::PackedSkeletalMesh phoenix_compat::pack_softskin_mesh(const phoenix::softskin_mesh& self) {
  ZenLoad::PackedSkeletalMesh mesh {};
  std::vector<ZenLoad::SkeletalVertex> vertices(self.mesh().positions().size());

  auto bb = get_total_aabb(self);
  mesh.bbox[0] = {bb.min.x, bb.min.y, bb.min.z};
  mesh.bbox[1] = {bb.max.x, bb.max.y, bb.max.z};

  // Extract weights and local positions
  const auto& stream = self.weights();

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
  for(size_t s=0; s<self.mesh().submeshes().size(); s++) {
    const auto& sm = self.mesh().submeshes()[s];
    vboSize += sm.wedges.size();
    iboSize += sm.triangles.size()*3;
  }

  mesh.vertices.resize(vboSize);
  mesh.indices .resize(iboSize);
  mesh.subMeshes.resize(self.mesh().submeshes().size());
  auto* vbo = mesh.vertices.data();
  auto* ibo = mesh.indices.data();

  uint32_t meshVxStart = 0, iboStart = 0;
  for(size_t s=0; s<self.mesh().submeshes().size(); s++) {
    const auto& sm = self.mesh().submeshes()[s];
    // Get data
    for(size_t i=0; i<sm.wedges.size(); ++i) {
      const auto&  wedge = sm.wedges[i];
      ZenLoad::SkeletalVertex v   = vertices[wedge.index];

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
    pack.material    = to_zenlib_material(sm.mat);
    meshVxStart += uint32_t(sm.wedges.size());
    iboStart    += uint32_t(sm.triangles.size()*3);
  }

  return mesh;
}
