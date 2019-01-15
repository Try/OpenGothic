#include "world.h"

#include "gothic.h"
#include "resources.h"

#include <zenload/zCMesh.h>
#include <fstream>

World::World(Gothic& gothic,std::string file)
  :name(std::move(file)),gothic(&gothic) {
  ZenLoad::ZenParser parser(name,Resources::vdfsIndex());

  // TODO: update loader
  parser.readHeader();

  // TODO: update loader
  ZenLoad::oCWorldData world;
  parser.readWorld(world);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 0.01f, false);
  if(0){
    std::ofstream out("debug_pmesh.obj");
    Gothic::debug(mesh,out);
    }

  static_assert(sizeof(Resources::LandVertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::LandVertex* vert=reinterpret_cast<const Resources::LandVertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::LandVertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    blocks.emplace_back();
    Block& b = blocks.back();

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }
  }
