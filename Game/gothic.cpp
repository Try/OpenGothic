#include "gothic.h"

#include <zenload/zCMesh.h>
#include <cstring>
#include "utils/installdetect.h"

// rate 14.5 to 1
const uint64_t Gothic::multTime=29;
const uint64_t Gothic::divTime =2;

Gothic::Gothic(const int argc, const char **argv) {
  //setWorld(std::make_unique<World>());
  if(argc<1)
    return;

  for(int i=1;i<argc;++i){
    if(std::strcmp(argv[i],"-g")==0){
      ++i;
      if(i<argc)
        gpath.assign(argv[i],argv[i]+std::strlen(argv[i]));
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

  if(gpath.empty()){
    InstallDetect inst;
    gpath = inst.detectG2();
    }

  for(auto& i:gpath)
    if(i=='\\')
      i='/';

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');

  if(wdef.empty()){
    if(isGothic2())
      wdef = "newworld.zen"; else
      wdef = "world.zen";
    }
  setTime(gtime(8,0));
  }

bool Gothic::isGothic2() const {
  // check actually for gothic-1, any questionable case is g2notr
  return gpath.find(u"Gothic/")==std::string::npos && gpath.find(u"gothic/")==std::string::npos;
  }

bool Gothic::isInGame() const {
  return wrld!=nullptr && !wrld->isEmpty();
  }

void Gothic::setWorld(std::unique_ptr<World> &&w) {
  wrld = std::move(w);
  }

std::unique_ptr<World> Gothic::clearWorld() {
  if(wrld)
    wrld->view()->resetCmd();
  return std::move(wrld);
  }

WorldView *Gothic::worldView() const {
  if(wrld)
    return wrld->view();
  return nullptr;
  }

Npc *Gothic::player() {
  if(wrld)
    return wrld->player();
  return nullptr;
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

Gothic::LoadState Gothic::checkLoading() {
  return loadingFlag.load();
  }

bool Gothic::finishLoading() {
  auto two=LoadState::Finalize;
  if(loadingFlag.compare_exchange_strong(two,LoadState::Idle)){
    loaderTh.join();
    return true;
    }
  return false;
  }

void Gothic::startLoading(const std::function<void()> f) {
  auto zero=LoadState::Idle;
  if(!loadingFlag.compare_exchange_strong(zero,LoadState::Loading)){
    return; // loading already
    }

  auto l = std::thread([this,f](){
    f();
    auto one=LoadState::Loading;
    loadingFlag.compare_exchange_strong(one,LoadState::Finalize);
    });
  loaderTh=std::move(l);
  }

void Gothic::cancelLoading() {
  if(loadingFlag.load()!=LoadState::Idle){
    loaderTh.join();
    loadingFlag.store(LoadState::Idle);
    }
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
  if(wrld)
    wrld->updateAnimation();
  }

std::vector<WorldScript::DlgChoise> Gothic::updateDialog(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return wrld->updateDialog(dlg,player,npc);
  }

void Gothic::dialogExec(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  wrld->exec(dlg,player,npc);
  }

void Gothic::aiProcessInfos(Npc& player,Npc &npc) {
  onDialogProcess(player,npc);
  }

bool Gothic::aiOuput(Npc &player, const char *msg) {
  bool ret=false;
  onDialogOutput(player,msg,ret);
  return ret;
  }

void Gothic::aiForwardOutput(Npc &player, const char *msg) {
  onDialogForwardOutput(player,msg);
  }

bool Gothic::aiCloseDialog() {
  bool ret=false;
  onDialogClose(ret);
  return ret;
  }

bool Gothic::aiIsDlgFinished() {
  bool v=true;
  isDialogClose(v);
  return v;
  }

void Gothic::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  onPrintScreen(msg,x,y,time,font);
  }

void Gothic::print(const char *msg) {
  onPrint(msg);
  }

const std::string &Gothic::messageByName(const std::string &id) const {
  if(!wrld){
    static std::string empty;
    return empty;
    }
  return wrld->script()->messageByName(id);
  }

uint32_t Gothic::messageTime(const std::string &id) const {
  if(!wrld){
    return 0;
    }
  return wrld->script()->messageTime(id);
  }

const std::string &Gothic::defaultWorld() const {
  return wdef;
  }

std::unique_ptr<Daedalus::DaedalusVM> Gothic::createVm(const char16_t *datFile) {
  Tempest::RFile dat(gpath+datFile);
  size_t all=dat.size();

  std::unique_ptr<uint8_t[]> byte(new uint8_t[all]);
  dat.read(byte.get(),all);

  auto vm = std::make_unique<Daedalus::DaedalusVM>(byte.get(),all);
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
