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

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    blocks.emplace_back();
    Block& b = blocks.back();

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){ return a.texture<b.texture; });

  for(auto& vob:world.rootVobs)
    loadVob(vob);
  }

void World::loadVob(const ZenLoad::zCVobData &vob) {
  for(auto& i:vob.childVobs)
    loadVob(i);

  static const float mult=0.01f;
//  static const float mult=0.1f;
//  static const float mult=0.1f;

  if(!vob.visual.empty()){
    Dodad d;
    d.mesh = Resources::loadStMesh(vob.visual);
    d.objMat.identity();
    d.objMat.translate(vob.position.x*mult,vob.position.y*mult,vob.position.z*mult);

    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));

    d.objMat = Tempest::Matrix4x4(v);
    d.objMat.set(3,0,vob.position.x*mult);
    d.objMat.set(3,1,vob.position.y*mult);
    d.objMat.set(3,2,vob.position.z*mult);

    staticObj.emplace_back(d);
    }
  }
