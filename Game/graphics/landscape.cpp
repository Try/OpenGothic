#include "landscape.h"

#include <Tempest/Log>

#include "rendererstorage.h"
#include "graphics/submesh/packedmesh.h"
#include "graphics/dynamic/painter3d.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(WorldView& owner, const SceneGlobals &scene, const PackedMesh &mesh)
  :owner(owner), scene(scene) {
  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::vbo<Resources::Vertex>(vert,mesh.vertices.size());

  Bounds                bbox;
  Matrix4x4             ident;
  ident.identity();

  for(auto& i:mesh.subMeshes) {
    auto material = Resources::loadMaterial(i.material);
    if(material.alpha==Material::AdditiveLight || i.indices.size()==0)
      continue;
    if(material.alpha==Material::InvalidAlpha) {
      Log::i("unrecognized alpha func: ",i.material.alphaFunc);
      continue;
      }

    if(material.tex==nullptr || material.tex->isEmpty())
      continue;

    bbox.assign(mesh.vertices,i.indices);
    blocks.emplace_back();
    auto& b = blocks.back();
    b.ibo  = Resources::ibo(i.indices.data(),i.indices.size());
    b.mesh = owner.getLand(vbo,b.ibo,material,bbox);
    b.mesh.setObjMatrix(ident);
    }
  }
