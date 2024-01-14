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

  meshletDescBase = Resources::ssbo(packed.meshletBounds.data(),packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));

  auto meshletDescCpu = packed.meshletBounds;
  blocks.reserve(packed.subMeshes.size());
  for(size_t i=0; i<packed.subMeshes.size(); ++i) {
    auto& sub      = packed.subMeshes[i];
    auto  id       = uint32_t(sub.iboOffset/PackedMesh::MaxInd);
    auto  len      = uint32_t(sub.iboLength/PackedMesh::MaxInd);
    auto  material = Resources::loadMaterial(sub.material,true);

    if(material.alpha==Material::AdditiveLight || sub.iboLength==0) {
      for(size_t r=0; r<len; ++r) {
        meshletDescCpu[id+r].r        = -1.f;
        meshletDescCpu[id+r].bucketId = 0;
        }
      continue;
      }

    if(Gothic::options().doRayQuery) {
      mesh.sub[i].blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
      }

    if(material.alpha==Material::Solid || material.alpha==Material::AlphaTest) {
      Block b;
      b.draw = visual.getDr(mesh,material,sub.iboOffset,sub.iboLength,meshletDesc,DrawStorage::Landscape);
      b.draw.setObjMatrix(Matrix4x4::mkIdentity());
      for(size_t r=0; r<len; ++r) {
        meshletDescCpu[id+r].commandColorId = b.draw.commandId();
        meshletDescCpu[id+r].commandDepthId = 0;
        meshletDescCpu[id+r].bucketId       = b.draw.bucketId();
        }
      blocks.emplace_back(std::move(b));
      continue;
      }

    // non-GDR
    for(size_t r=0; r<len; ++r) {
      meshletDescCpu[id+r].r        = -1.f;
      meshletDescCpu[id+r].bucketId = 0;
      }

    Bounds bbox;
    bbox.assign(packed.vertices,packed.indices,sub.iboOffset,sub.iboLength);

    Block b;
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,meshletDescBase,bbox,ObjectsBucket::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }

  meshletDesc = Resources::ssbo(meshletDescCpu.data(),meshletDescCpu.size()*sizeof(meshletDescCpu[0]));
  }
