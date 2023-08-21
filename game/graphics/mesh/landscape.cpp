#include "landscape.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  if(Gothic::options().doMeshShading)
    meshletDesc = Resources::ssbo(packed.meshletBounds.data(),packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));

  auto& device = Resources::device();

  blocks.reserve(packed.subMeshes.size());
  for(size_t i=0; i<packed.subMeshes.size(); ++i) {
    auto& sub      = packed.subMeshes[i];
    auto  material = Resources::loadMaterial(sub.material,true);
    if(material.alpha==Material::AdditiveLight || sub.iboLength==0)
      continue;

    if(material.tex==nullptr || material.tex->isEmpty())
      continue;

    if(Gothic::options().doRayQuery) {
      mesh.sub[i].blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
      }

    Bounds bbox;
    bbox.assign(packed.vertices,packed.indices,sub.iboOffset,sub.iboLength);

    Block b;
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,meshletDesc,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }

  if(Gothic::options().doMeshShading) {
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
                                  meshletSolidDesc,Bounds(),ObjectsBucket::LandscapeShadow);
    }
  }
