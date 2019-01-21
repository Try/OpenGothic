#include "animmesh.h"

AnimMesh::AnimMesh(const ZenLoad::PackedSkeletalMesh &mesh) {
  static_assert(sizeof(Vertex)==sizeof(ZenLoad::SkeletalVertex),"invalid landscape vertex format");
  const Vertex* vert=reinterpret_cast<const Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Vertex>(vert,mesh.vertices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texture = Resources::loadTexture(mesh.subMeshes[i].material.texture);
    sub[i].ibo     = Resources::loadIbo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  }
