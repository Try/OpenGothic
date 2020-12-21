#include "animmesh.h"

#include <Tempest/Log>

static size_t countBones(const ZenLoad::SkeletalVertex* v, size_t n) {
  if(n==0)
    return 0;
  uint8_t ret = 0;
  for(size_t i=0; i<n; ++i) {
    ret = std::max(ret, v[i].BoneIndices[0]);
    ret = std::max(ret, v[i].BoneIndices[1]);
    ret = std::max(ret, v[i].BoneIndices[2]);
    ret = std::max(ret, v[i].BoneIndices[3]);
    }
  return ret+1;
  }

AnimMesh::AnimMesh(const ZenLoad::PackedSkeletalMesh &mesh)
  : bonesCount(countBones(mesh.vertices.data(),mesh.vertices.size())) {
  static_assert(sizeof(VertexA)==sizeof(mesh.vertices[0]),"invalid VertexA size");
  const VertexA* vert = reinterpret_cast<const VertexA*>(mesh.vertices.data());
  vbo = Resources::vbo(vert,mesh.vertices.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texName  = mesh.subMeshes[i].material.texture;
    sub[i].material = Resources::loadMaterial(mesh.subMeshes[i].material,true);
    sub[i].ibo      = Resources::ibo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  bbox.assign(mesh.bbox);
  }
