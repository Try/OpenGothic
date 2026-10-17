#include "landscape.h"

#include <Tempest/Log>
#include <cstddef>

#include "graphics/shaders.h"

using namespace Tempest;

Landscape::Mesh::Mesh(const PackedMesh& packed)
  :mesh(packed), meshletBounds(packed.meshletBounds) {
  auto& device = Resources::device();

  meshletDesc = Resources::ssbo(packed.meshletBounds.data(), packed.meshletBounds.size()*sizeof(packed.meshletBounds[0]));
  bvhNodes    = Resources::ssbo(packed.bvhNodes.data(),  packed.bvhNodes.size()*sizeof(packed.bvhNodes[0]));
  //bvhNodes    = Resources::ssbo(packed.bvh8Nodes.data(), packed.bvh8Nodes.size()*sizeof(packed.bvh8Nodes[0]));

  for(auto& sub:mesh.sub) {
    if(sub.material.alpha==Material::AdditiveLight || sub.iboLength==0)
      continue;
    if(Shaders::options().doRtScene)
      sub.blas = device.blas(mesh.vbo,mesh.ibo,sub.iboOffset,sub.iboLength);
    }
  }

Landscape::Landscape(VisualObjects& visual, std::unique_ptr<Mesh> m)
  :data(std::move(m)) {
  auto& mesh = data->mesh;
  blocks.reserve(mesh.sub.size());
  for(auto& sub:mesh.sub) {
    auto  id       = uint32_t(sub.iboOffset/PackedMesh::MaxInd);
    auto& material = sub.material;

    if(material.alpha==Material::AdditiveLight || sub.iboLength==0) {
      continue;
      }

    Block b;
    b.mesh = visual.get(mesh,material,sub.iboOffset,sub.iboLength,&data->meshletBounds[id],DrawCommands::Landscape);
    b.mesh.setObjMatrix(Matrix4x4::mkIdentity());
    blocks.emplace_back(std::move(b));
    }
  }

auto Landscape::takeMesh() -> std::unique_ptr<Mesh> {
  blocks.clear();
  return std::move(data);
  }
