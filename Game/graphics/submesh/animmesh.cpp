#include "animmesh.h"

#include <Tempest/Log>

AnimMesh::AnimMesh(const ZenLoad::PackedSkeletalMesh &mesh) {
  /*
  std::vector<VertexA> cvbo(mesh.vertices.size());
  for(size_t i=0;i<cvbo.size();++i){
    cvbo[i].color   = mesh.vertices[i].Color;

    cvbo[i].norm[0] = mesh.vertices[i].Normal.x;
    cvbo[i].norm[1] = mesh.vertices[i].Normal.y;
    cvbo[i].norm[2] = mesh.vertices[i].Normal.z;

    cvbo[i].uv[0]   = mesh.vertices[i].TexCoord.x;
    cvbo[i].uv[1]   = mesh.vertices[i].TexCoord.y;

    cvbo[i].pos[0]  = mesh.vertices[i].LocalPositions[0].x;
    cvbo[i].pos[1]  = mesh.vertices[i].LocalPositions[0].y;
    cvbo[i].pos[2]  = mesh.vertices[i].LocalPositions[0].z;
    }*/
  static_assert(sizeof(VertexA)==sizeof(ZenLoad::SkeletalVertex),"invalid vertex format");
  const VertexA* vert=reinterpret_cast<const VertexA*>(mesh.vertices.data());
  vbo = Resources::loadVbo<VertexA>(vert,mesh.vertices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texture = Resources::loadTexture(mesh.subMeshes[i].material.texture);
    sub[i].ibo     = Resources::loadIbo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  }
