#include "landscape.h"

#include <Tempest/Log>
#include <cstddef>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/shaders.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  auto& device = Resources::device();

  meshletDesc = Resources::ssbo(packed.meshletBounds.data(),packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));

  blocks.reserve(packed.subMeshes.size());
  for(size_t i=0; i<packed.subMeshes.size(); ++i) {
    auto& sub      = packed.subMeshes[i];
    auto  id       = uint32_t(sub.iboOffset/PackedMesh::MaxInd);
    auto  material = Resources::loadMaterial(sub.material,true);

    if(material.alpha==Material::AdditiveLight || sub.iboLength==0) {
      continue;
      }

    if(Gothic::options().doRayQuery) {
      mesh.sub[i].blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
      }

    Block b;
    b.draw = visual.get(mesh,material,sub.iboOffset,sub.iboLength,&packed.meshletBounds[id],DrawStorage::Landscape);
    b.draw.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }
  }
