#include "landscape.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  if(Resources::hasMeshShaders())
    meshletDesc = Resources::ssbo(packed.meshletBounds.data(),packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));

  auto& device = Resources::device();
  std::vector<uint32_t> ibo;
  blocks.reserve(packed.subMeshes.size());
  for(auto& i:packed.subMeshes) {
    auto material = Resources::loadMaterial(i.material,true);
    if(material.alpha==Material::AdditiveLight || i.iboLength==0)
      continue;

    if(material.tex==nullptr || material.tex->isEmpty())
      continue;

    Block b;
    if(Gothic::inst().doRayQuery()) {
      if(material.alpha!=Material::Solid) {
        b.blas = device.blas(mesh.vbo,mesh.ibo,i.iboOffset,i.iboLength);
        } else {
        size_t iboSz = ibo.size();
        ibo.resize(ibo.size()+i.iboLength);
        for(size_t id=0; id<i.iboLength; ++id)
          ibo[iboSz+id] = packed.indices[i.iboOffset+id];
        // TODO: proper blas support
        }
      }

    Bounds bbox;
    bbox.assign(packed.vertices,packed.indices,i.iboOffset,i.iboLength);
    b.mesh = visual.get(mesh,material,i.iboOffset,i.iboLength,
                        &b.blas,meshletDesc,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }

  if(Gothic::inst().doRayQuery() && ibo.size()>0) {
    rt.ibo  = Resources::ibo(ibo.data(),ibo.size());
    rt.blas = device.blas(mesh.vbo,rt.ibo);
    }

  if(Resources::hasMeshShaders()) {
    auto boundsSolid = packed.meshletBounds;
    for(auto& i:packed.subMeshes) {
      auto material = Resources::loadMaterial(i.material,true);
      if(material.alpha!=Material::Solid) {
        size_t id  = (i.iboOffset/PackedMesh::MaxInd);
        size_t len = (i.iboLength/PackedMesh::MaxInd);
        for(size_t r=0; r<len; ++r)
          boundsSolid[id+r].r = -1.f;
        }
      }

    Material matSh;
    matSh.tex   = &Resources::fallbackBlack();
    matSh.alpha = Material::Solid;
    meshletSolidDesc = Resources::ssbo(boundsSolid.data(), boundsSolid.size()*sizeof(boundsSolid[0]));
    solidBlock.mesh  = visual.get(mesh,matSh,0,mesh.ibo.size(),
                                  nullptr,meshletSolidDesc,Bounds(),ObjectsBucket::LandscapeShadow);
    }
  }
