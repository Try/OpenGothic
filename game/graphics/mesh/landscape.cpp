#include "landscape.h"

#include <Tempest/Log>
#include <cstddef>

#include "graphics/mesh/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(VisualObjects& visual, const PackedMesh &packed)
  :mesh(packed) {
  auto& device = Resources::device();

  meshletDesc = Resources::ssbo(packed.meshletBounds.data(),packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));
  bvhNodes    = Resources::ssbo(packed.bvhNodes.data(),packed.bvhNodes.size()*sizeof(packed.bvhNodes[0]));

  // bvhNodes64  = Resources::ssbo(packed.bvhNodes64.data(),packed.bvhNodes64.size()*sizeof(packed.bvhNodes64[0]));
  // bvh64Ibo    = Resources::device().ssbo(packed.bvh64Ibo);
  // bvh64Vbo    = Resources::device().ssbo(packed.bvh64Vbo);

  bvh8Nodes = Resources::ssbo(packed.bvh8Nodes.data(), packed.bvh8Nodes.size()*sizeof(packed.bvh8Nodes[0]));

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
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,&packed.meshletBounds[id],DrawCommands::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }
  }
