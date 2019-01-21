#include "gothic.h"

#include <zenload/zCMesh.h>
#include <cstring>

Gothic::Gothic(const int argc, const char **argv) {
  if(argc<1)
    return;

  for(int i=1;i<argc;++i){
    if(std::strcmp(argv[i],"-g")==0){
      ++i;
      if(i<argc) {
        gpath=argv[i];
        for(auto& i:gpath)
          if(i=='\\')
            i='/';
        }
      }
    }

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');

  wdef = "oldworld.zen";
  }

bool Gothic::isInGame() const {
  return !wrld.isEmpty();
  }

void Gothic::setWorld(World &&w) {
  wrld = std::move(w);
  onWorldChanged();
  }

const std::string &Gothic::defaultWorld() const {
  return wdef;
  }

std::unique_ptr<Daedalus::DaedalusVM> Gothic::createVm(const char *datFile) {
  auto vm = std::make_unique<Daedalus::DaedalusVM>(gpath+datFile);
  Daedalus::registerGothicEngineClasses(*vm);
  return vm;
  }

void Gothic::debug(const ZenLoad::zCMesh &mesh, std::ostream &out) {
  for(auto& i:mesh.getVertices())
    out << "v " << i.x << " " << i.y << " " << i.z << std::endl;
  for(size_t i=0;i<mesh.getIndices().size();i+=3){
    const uint32_t* tri = &mesh.getIndices()[i];
    out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
    }
  }

void Gothic::debug(const ZenLoad::PackedMesh &mesh, std::ostream &out) {
  for(auto& i:mesh.vertices) {
    out << "v  " << i.Position.x << " " << i.Position.y << " " << i.Position.z << std::endl;
    out << "vn " << i.Normal.x   << " " << i.Normal.y   << " " << i.Normal.z   << std::endl;
    out << "vt " << i.TexCoord.x << " " << i.TexCoord.y  << std::endl;
    }

  for(auto& sub:mesh.subMeshes){
    out << "o obj_" << int(&sub-&mesh.subMeshes[0]) << std::endl;
    for(size_t i=0;i<sub.indices.size();i+=3){
      const uint32_t* tri = &sub.indices[i];
      out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
      }
    }
  }

void Gothic::debug(const ZenLoad::PackedSkeletalMesh &mesh, std::ostream &out) {
  for(auto& i:mesh.vertices) {
    out << "v  " << i.LocalPositions[0].x << " " << i.LocalPositions[0].y << " " << i.LocalPositions[0].z << std::endl;
    out << "vn " << i.Normal.x   << " " << i.Normal.y   << " " << i.Normal.z   << std::endl;
    out << "vt " << i.TexCoord.x << " " << i.TexCoord.y  << std::endl;
    }

  for(auto& sub:mesh.subMeshes){
    out << "o obj_" << int(&sub-&mesh.subMeshes[0]) << std::endl;
    for(size_t i=0;i<sub.indices.size();i+=3){
      const uint32_t* tri = &sub.indices[i];
      out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
      }
    }
  }
