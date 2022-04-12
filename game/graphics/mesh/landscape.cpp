#include "landscape.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &mesh) {
  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::vbo<Resources::Vertex>(vert,mesh.vertices.size());

  auto& device = Resources::device();

  Bounds bbox;
  std::vector<uint32_t> ibo;

  for(auto& i:mesh.subMeshes) {
    auto material = Resources::loadMaterial(i.material,true);
    if(material.alpha==Material::AdditiveLight || i.indices.size()==0)
      continue;

    if(material.tex==nullptr || material.tex->isEmpty())
      continue;

    bbox.assign(mesh.vertices,i.indices);
    blocks.emplace_back();
    auto& b = blocks.back();
    b.ibo  = Resources::ibo(i.indices.data(),i.indices.size());

    if(Gothic::inst().doRayQuery() && material.alpha!=Material::Solid)
      b.blas = device.blas(vbo,b.ibo); else
      ibo.insert(ibo.end(), i.indices.begin(),i.indices.end());
    b.mesh = visual.get(vbo,b.ibo,&b.blas,material,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    }

  if(Gothic::inst().doRayQuery()) {
    iboSolid  = Resources::ibo(ibo.data(),ibo.size());
    blasSolid = device.blas(vbo,iboSolid);
    }
  }
