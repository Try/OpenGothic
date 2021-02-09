#include "landscape.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/dynamic/painter3d.h"
#include "graphics/rendererstorage.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(WorldView& owner, VisualObjects& visual, const PackedMesh &mesh)
  :owner(owner), visual(visual) {
  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::vbo<Resources::Vertex>(vert,mesh.vertices.size());

  Bounds    bbox;
  Matrix4x4 ident;
  ident.identity();

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
    b.mesh = visual.get(vbo,b.ibo,material,bbox);
    b.mesh.setObjMatrix(ident);
    }
  }
