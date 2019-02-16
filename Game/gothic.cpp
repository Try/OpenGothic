#include "gothic.h"

#include <zenload/zCMesh.h>
#include <cstring>

// rate 14.5 to 1
const uint64_t Gothic::multTime=29;
const uint64_t Gothic::divTime =2;

Gothic::Gothic(const int argc, const char **argv) {
  setWorld(std::make_unique<World>());
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
    else if(std::strcmp(argv[i],"-w")==0){
      ++i;
      if(i<argc)
        wdef=argv[i];
      }
    else if(std::strcmp(argv[i],"-nomenu")==0){
      noMenu=true;
      }
    }

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');

  if(wdef.empty())
    wdef = "oldworld.zen";
  setTime(gtime(8,0));
  }

bool Gothic::isInGame() const {
  return !wrld->isEmpty();
  }

void Gothic::setWorld(std::unique_ptr<World> &&w) {
  wrld = std::move(w);
  }

void Gothic::pushPause() {
  pauseSum++;
  }

void Gothic::popPause() {
  pauseSum--;
  }

bool Gothic::isPause() const {
  return pauseSum;
  }

uint64_t Gothic::tickCount() const {
  return ticks;
  }

void Gothic::tick(uint64_t dt) {
  ticks+=dt;

  uint64_t add = (dt+wrldTimePart)*multTime;
  wrldTimePart=add%divTime;

  wrldTime.addMilis(add/divTime);
  wrld->tick(dt);
  }

void Gothic::setTime(gtime t) {
  wrldTime=t;
  }

void Gothic::updateAnimation() {
  wrld->updateAnimation();
  }

std::vector<WorldScript::DlgChoise> Gothic::updateDialog(const WorldScript::DlgChoise &dlg) {
  return wrld->updateDialog(dlg);
  }

void Gothic::dialogExec(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  wrld->exec(dlg,player,npc);
  }

void Gothic::aiOuput(const char *msg) {
  onDialogOutput(msg);
  }

void Gothic::aiCloseDialog() {
  onDialogClose();
  }

void Gothic::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  onPrintScreen(msg,x,y,time,font);
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
