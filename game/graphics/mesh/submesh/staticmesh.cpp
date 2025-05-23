#include "staticmesh.h"

#include <cassert>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

StaticMesh::StaticMesh(const PackedMesh& mesh) {
  const Vertex* vert=mesh.vertices.data();
  vbo  = Resources::vbo<Vertex>  (vert,mesh.vertices.size());
  ibo  = Resources::ibo<uint32_t>(mesh.indices.data(),mesh.indices.size());
  ibo8 = Resources::ssbo(mesh.indices8.data(),mesh.indices8.size());

  sub.resize(mesh.subMeshes.size());
  for(size_t i=0;i<mesh.subMeshes.size();++i) {
    sub[i].texName   = mesh.subMeshes[i].material.texture;
    sub[i].material  = Resources::loadMaterial(mesh.subMeshes[i].material,mesh.isUsingAlphaTest);
    sub[i].iboOffset = mesh.subMeshes[i].iboOffset;
    sub[i].iboLength = mesh.subMeshes[i].iboLength;
    }
  bbox.assign(mesh.bbox());

  if(Gothic::options().doRayQuery) {
    for(size_t i=0;i<mesh.subMeshes.size();++i) {
      sub[i].blas = Resources::blas(vbo, ibo, sub[i].iboOffset, sub[i].iboLength);
      }
    }
  }

const Tempest::AccelerationStructure* StaticMesh::blas(size_t iboOffset, size_t iboLen) const {
  for(auto& i:sub)
    if(i.iboOffset==iboOffset && i.iboLength==iboLen && !i.blas.isEmpty()) {
      return &i.blas;
      }
  return nullptr;
  }

StaticMesh::StaticMesh(const Material& mat, std::vector<Resources::Vertex> cvbo, std::vector<uint32_t> cibo) {
  assert(cvbo.size()<=PackedMesh::MaxVert);
  assert(cibo.size()<=PackedMesh::MaxInd);

  if(Gothic::options().doMeshShading || true) {
    const size_t vert = cvbo.size();
    const size_t prim = cibo.size()/3;

    cvbo.resize(PackedMesh::MaxVert);
    std::vector<uint8_t> ibo(PackedMesh::MaxPrim*4);
    for(size_t i=0; i<cibo.size(); i+=3) {
      size_t at = (i/3)*4;
      ibo[at+0] = uint8_t(cibo[i+0]);
      ibo[at+1] = uint8_t(cibo[i+1]);
      ibo[at+2] = uint8_t(cibo[i+2]);
      ibo[at+3] = 0;
      }
    size_t at = (PackedMesh::MaxPrim)*4 - 4;
    ibo[at + 0] = uint8_t(0);
    ibo[at + 1] = uint8_t(0);
    ibo[at + 2] = uint8_t(prim);
    ibo[at + 3] = uint8_t(vert);

    ibo8 = Resources::ssbo(ibo.data(),ibo.size());
    }
  vbo  = Resources::vbo<Vertex>(cvbo.data(),cvbo.size());
  ibo  = Resources::ibo(cibo.data(),cibo.size());

  sub.resize(1);
  for(size_t i=0;i<1;++i) {
    sub[i].texName   = "";
    sub[i].material  = mat;
    //sub[i].iboLength = Gothic::options().doMeshShading ? PackedMesh::MaxInd : ibo.size();
    sub[i].iboLength = PackedMesh::MaxInd;
    if(Gothic::options().doRayQuery)
      sub[i].blas = Resources::blas(vbo, ibo, 0, ibo.size());
    }
  bbox.assign(cvbo);
  }
