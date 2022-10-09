#include "staticmesh.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

StaticMesh::StaticMesh(const PackedMesh& mesh) {
  const Vertex* vert=mesh.vertices.data();
  vbo = Resources::vbo<Vertex>  (vert,mesh.vertices.size());
  ibo = Resources::ibo<uint32_t>(mesh.indices.data(),mesh.indices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i) {
    sub[i].texName   = mesh.subMeshes[i].material.texture;
    sub[i].material  = Resources::loadMaterial(mesh.subMeshes[i].material,mesh.isUsingAlphaTest);
    sub[i].iboOffset = mesh.subMeshes[i].iboOffset;
    sub[i].iboLength = mesh.subMeshes[i].iboLength;
    }
  bbox.assign(mesh.bbox());

  if(Gothic::inst().doRayQuery()) {
    for(size_t i=0;i<mesh.subMeshes.size();++i) {
      sub[i].blas = Resources::blas(vbo,ibo, sub[i].iboOffset, sub[i].iboLength);
      }
    }
  }

StaticMesh::StaticMesh(const Material& mat, std::vector<Resources::Vertex> cvbo, std::vector<uint32_t> cibo) {
  vbo = Resources::vbo<Vertex>(cvbo.data(),cvbo.size());
  ibo = Resources::ibo(cibo.data(),cibo.size());
  sub.resize(1);
  for(size_t i=0;i<1;++i) {
    sub[i].texName   = "";
    sub[i].material  = mat;
    sub[i].iboLength = ibo.size();
    if(Gothic::inst().doRayQuery())
      sub[i].blas = Resources::blas(vbo,ibo,0,ibo.size());
    }
  bbox.assign(cvbo);
  }
