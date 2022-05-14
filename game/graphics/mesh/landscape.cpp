#include "landscape.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &mesh) {
  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());

  vbo = Resources::vbo<Resources::Vertex>(vert,mesh.vertices.size());
  ibo = Resources::ibo(mesh.indices.data(),mesh.indices.size());

  auto& device = Resources::device();

  Bounds bbox;
  std::vector<uint32_t> iboSolid;

  if(Resources::hasMeshShaders()) {
    meshletDesc = Resources::ssbo(mesh.meshletBounds.data(),mesh.meshletBounds.size()*sizeof(mesh.meshletBounds[0]));
    }

  for(auto& i:mesh.subMeshes) {
    auto material = Resources::loadMaterial(i.material,true);
    if(material.alpha==Material::AdditiveLight || i.iboLength==0)
      continue;

    if(material.tex==nullptr || material.tex->isEmpty())
      continue;

    bbox.assign(mesh.vertices,mesh.indices,i.iboOffset,i.iboLength);
    blocks.emplace_back();
    auto& b = blocks.back();
    b.iboOffset = i.iboOffset;
    b.iboLength = i.iboLength;

    if(Gothic::inst().doRayQuery()) {
      if(material.alpha!=Material::Solid) {
        b.blas = device.blas(vbo,ibo,i.iboOffset,i.iboLength);
        } else {
        size_t iboSz = iboSolid.size();
        iboSolid.resize(iboSolid.size()+i.iboLength);
        for(size_t id=0; id<i.iboLength; ++id)
          iboSolid[iboSz+id] = mesh.indices[i.iboOffset+id];
        // TODO: proper blas support
        }
      }

    b.mesh = visual.get(vbo,ibo,i.iboOffset,i.iboLength,&b.blas,&meshletDesc,material,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    }

  if(Gothic::inst().doRayQuery() && iboSolid.size()>0) {
    this->iboSolid = Resources::ibo(iboSolid.data(),iboSolid.size());
    blasSolid      = device.blas(vbo,this->iboSolid);
    }
  }
