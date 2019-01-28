#include "animmesh.h"

#include <Tempest/Log>

AnimMesh::AnimMesh(const ZenLoad::PackedSkeletalMesh &mesh) {
  std::vector<VertexA> cvbo(mesh.vertices.size());
  for(size_t i=0;i<cvbo.size();++i){
    cvbo[i].pos[0]  = mesh.vertices[i].LocalPositions[0].x;
    cvbo[i].pos[1]  = mesh.vertices[i].LocalPositions[0].y;
    cvbo[i].pos[2]  = mesh.vertices[i].LocalPositions[0].z;

    cvbo[i].norm[0] = mesh.vertices[i].Normal.x;
    cvbo[i].norm[1] = mesh.vertices[i].Normal.y;
    cvbo[i].norm[2] = mesh.vertices[i].Normal.z;

    cvbo[i].uv[0]   = mesh.vertices[i].TexCoord.x;
    cvbo[i].uv[1]   = mesh.vertices[i].TexCoord.y;

    cvbo[i].weights[0] = mesh.vertices[i].Weights[0];
    cvbo[i].weights[1] = mesh.vertices[i].Weights[1];
    cvbo[i].weights[2] = mesh.vertices[i].Weights[2];
    cvbo[i].weights[3] = mesh.vertices[i].Weights[3];

    cvbo[i].boneId[0] = mesh.vertices[i].BoneIndices[0];
    cvbo[i].boneId[1] = mesh.vertices[i].BoneIndices[1];
    cvbo[i].boneId[2] = mesh.vertices[i].BoneIndices[2];
    cvbo[i].boneId[3] = mesh.vertices[i].BoneIndices[3];
    }

  vbo = Resources::loadVbo(cvbo.data(),cvbo.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i){
    sub[i].texture = Resources::loadTexture(mesh.subMeshes[i].material.texture);
    sub[i].ibo     = Resources::loadIbo(mesh.subMeshes[i].indices.data(),mesh.subMeshes[i].indices.size());
    }
  }
