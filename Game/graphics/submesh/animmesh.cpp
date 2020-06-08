#include "animmesh.h"

#include <Tempest/Log>

AnimMesh::AnimMesh(const ZenLoad::PackedSkeletalMesh &mesh) {
  static_assert(sizeof(VertexA)==sizeof(mesh.vertices[0]),"invalid VertexA size");
  const VertexA* vert = reinterpret_cast<const VertexA*>(mesh.vertices.data());
  vbo = Resources::vbo(vert,mesh.vertices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texName = mesh.subMeshes[i].material.texture;
    sub[i].texture = Resources::loadTexture(sub[i].texName);
    sub[i].ibo     = Resources::ibo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  bbox.assign(mesh.bbox);
  }
