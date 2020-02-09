#include "packedmesh.h"

#include <zenload/zCMaterial.h>

PackedMesh::PackedMesh(ZenLoad::zCMesh& mesh, PkgType type) {
  mesh.getBoundingBox(bbox[0],bbox[1]);
  if(type==PK_Visual) {
    subMeshes.resize(mesh.getMaterials().size());
    for(size_t i=0;i<subMeshes.size();++i)
      subMeshes[i].material = mesh.getMaterials()[i];
    }

  if(type==PK_Physic) {
    subMeshes.resize(ZenLoad::MaterialGroup::NUM_MAT_GROUPS);
    for(size_t i=0;i<subMeshes.size();++i)
      subMeshes[i].material.matGroup = uint8_t(i);
    }

  pack(mesh,type);
  }

void PackedMesh::pack(const ZenLoad::zCMesh& mesh,PkgType type) {
  auto& vbo = mesh.getVertices();
  auto& uv  = mesh.getFeatureIndices();
  auto& ibo = mesh.getIndices();

  struct Hash final {
    size_t operator() (const std::pair<uint32_t,uint32_t>& v) const noexcept {
      return v.first^v.second;
      }
    };

  vertices.reserve(vbo.size());

  std::unordered_map<std::pair<uint32_t,uint32_t>,size_t,Hash> icache;
  for(size_t i=0;i<ibo.size();++i) {
    size_t matId = submeshIndex(mesh,i,type);
    if(matId>=subMeshes.size())
      continue;
    auto& s = subMeshes[matId];

    std::pair<uint32_t,uint32_t> index={ibo[i], (type==PK_Physic ? 0 : uv[i])};
    auto r = icache.find(index);
    if(r!=icache.end()) {
      s.indices.push_back(uint32_t(r->second));
      } else {
      auto&       v  = mesh.getFeatures()[index.second];
      WorldVertex vx = {};

      vx.Position = vbo[index.first];
      vx.Normal   = v.vertNormal;
      vx.TexCoord = ZMath::float2(v.uv[0], v.uv[1]);
      vx.Color    = v.lightStat;

      size_t val = vertices.size();
      vertices.emplace_back(vx);
      s.indices.push_back(uint32_t(val));
      icache[index] = uint32_t(val);
      }
    }
  }

size_t PackedMesh::submeshIndex(const ZenLoad::zCMesh& mesh,size_t vertexId,PkgType type) const {
  auto&  mid = mesh.getTriangleMaterialIndices();
  size_t id  = size_t(mid[vertexId/3]);
  switch(type) {
    case PK_Visual:
      return id;
    case PK_Physic: {
      const auto&  mat = mesh.getMaterials()[id];
      if(mat.noCollDet)
        return size_t(-1);
      return size_t(mat.matGroup);
      }
    }
  return 0;
  }
