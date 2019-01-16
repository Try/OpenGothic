#include "staticmesh.h"

StaticMesh::StaticMesh(const ZenLoad::PackedMesh &mesh) {
  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texture = Resources::loadTexture(mesh.subMeshes[i].material.texture);
    sub[i].ibo     = Resources::loadIbo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  }
