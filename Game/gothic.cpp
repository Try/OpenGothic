#include "gothic.h"

#include <Tempest/Log>
#include <zenload/zCMesh.h>
#include <cstring>

#include "game/serialize.h"
#include "utils/installdetect.h"

Gothic::Gothic(const int argc, const char **argv) {
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
    else if(std::strcmp(argv[i],"-window")==0){
      isWindow=true;
      }
    else if(std::strcmp(argv[i],"-nomenu")==0){
      noMenu=true;
      }
    else if(std::strcmp(argv[i],"-rambo")==0){
      isRambo=true;
      }
    else if(std::strcmp(argv[i],"-validation")==0 || std::strcmp(argv[i],"-v")==0){
      isDebug=true;
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

  fight   .reset(new FightAi(*this));
  camera  .reset(new CameraDefinitions(*this));
  soundDef.reset(new SoundDefinitions(*this));
  music   .reset(new MusicDefinitions(*this));
  }

bool Gothic::isGothic2() const {
  // check actually for gothic-1, any questionable case is g2notr
  return gpath.find(u"Gothic/")==std::string::npos && gpath.find(u"gothic/")==std::string::npos;
  }

bool Gothic::isInGame() const {
  return game!=nullptr;
  }

const World *Gothic::world() const {
  if(game)
    return game->world();
  return nullptr;
  }

World *Gothic::world() {
  if(game)
    return game->world();
  return nullptr;
  }

void Gothic::setGame(std::unique_ptr<GameSession> &&w) {
  clearGame();
  game = std::move(w);
  }

std::unique_ptr<GameSession> Gothic::clearGame() {
  if(game)
    game->view()->resetCmd();
  return std::move(game);
  }

WorldView *Gothic::worldView() const {
  if(game)
    return game->view();
  return nullptr;
  }

Npc *Gothic::player() {
  if(game)
    return game->player();
  return nullptr;
  }

int Gothic::loadingProgress() const {
  return loadProgress.load();
  }

void Gothic::setLoadingProgress(int v) {
  loadProgress.store(v);
  }

const Tempest::Texture2d *Gothic::loadingBanner() const {
  return loadTex;
  }

SoundFx *Gothic::loadSoundFx(const char *name) {
  if(name==nullptr || *name=='\0')
    return nullptr;

  std::lock_guard<std::mutex> guard(syncSnd);
  auto it=sndFxCache.find(name);
  if(it!=sndFxCache.end())
    return &it->second;

  try {
    auto ret = sndFxCache.emplace(name,SoundFx(*this,name));
    return &ret.first->second;
    }
  catch(...){
    Tempest::Log::e("unable to load soundfx \"",name,"\"");
    return nullptr;
    }
  }

void Gothic::emitGlobalSound(const char *sfx) {
  emitGlobalSound(loadSoundFx(sfx));
  }

void Gothic::emitGlobalSound(const std::string &sfx) {
  emitGlobalSound(loadSoundFx(sfx.c_str()));
  }

void Gothic::emitGlobalSound(const SoundFx *sfx) {
  if(sfx!=nullptr){
    auto s = sfx->getGlobal(sndDev);
    s.play();

    for(size_t i=0;i<sndStorage.size();){
      if(sndStorage[i].isFinished()){
        sndStorage[i]=std::move(sndStorage.back());
        sndStorage.pop_back();
        } else {
        ++i;
        }
      }
    sndStorage.push_back(std::move(s));
    }
  }

void Gothic::emitGlobalSound(const Tempest::Sound &sfx) {
  auto s = sndDev.load(sfx);
  s.play();

  for(size_t i=0;i<sndStorage.size();){
    if(sndStorage[i].isFinished()){
      sndStorage[i]=std::move(sndStorage.back());
      sndStorage.pop_back();
      } else {
      ++i;
      }
    }
  sndStorage.push_back(std::move(s));
  }

void Gothic::emitGlobalSoundWav(const std::string &wav) {
  auto s = sndDev.load(Resources::loadSoundBuffer(wav));
  s.play();

  for(size_t i=0;i<sndStorage.size();){
    if(sndStorage[i].isFinished()){
      sndStorage[i]=std::move(sndStorage.back());
      sndStorage.pop_back();
      } else {
      ++i;
      }
    }
  sndStorage.push_back(std::move(s));
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

bool Gothic::isDebugMode() const {
  return isDebug;
  }

bool Gothic::isRamboMode() const {
  return isRambo;
  }

Gothic::LoadState Gothic::checkLoading() const {
  return loadingFlag.load();
  }

bool Gothic::finishLoading() {
  auto state = checkLoading();
  if(state!=LoadState::Finalize && state!=LoadState::Failed)
    return false;
  if(loadingFlag.compare_exchange_strong(state,LoadState::Idle)){
    loaderTh.join();
    if(pendingGame!=nullptr)
      game = std::move(pendingGame);
    onWorldLoaded();
    return true;
    }
  return false;
  }

void Gothic::startLoading(const char* banner, const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  loadTex = Resources::loadTexture(banner);
  loadProgress.store(0);

  auto zero=LoadState::Idle;
  if(!loadingFlag.compare_exchange_strong(zero,LoadState::Loading)){
    return; // loading already
    }

  auto g = clearGame().release();
  try{
    auto l = std::thread([this,f,g](){
      std::unique_ptr<GameSession> game(g);
      auto one=LoadState::Loading;
      try {
        auto next   = f(std::move(game));
        pendingGame = std::move(next);
        loadingFlag.compare_exchange_strong(one,LoadState::Finalize);
        }
      catch(std::bad_alloc&){
        Tempest::Log::e("loading error: out of memory");
        loadingFlag.compare_exchange_strong(one,LoadState::Failed);
        }
      });
    loaderTh=std::move(l);
    //loaderTh.join();
    //loaderTh = std::thread([](){});
    }
  catch(...){
    delete g; // delete manually, if we can't start thread
    }
  }

void Gothic::cancelLoading() {
  if(loadingFlag.load()!=LoadState::Idle){
    loaderTh.join();
    loadingFlag.store(LoadState::Idle);
    }
  }

void Gothic::tick(uint64_t dt) {
  game->tick(dt);
  }

void Gothic::updateAnimation() {
  if(game)
    game->updateAnimation();
  }

void Gothic::quickSave() {
  if(!game)
    return;

  Tempest::WFile f("qsave.sav");
  Serialize      s(f);
  game->save(s);

  print("Game saved"); //TODO: translation
  }

std::vector<GameScript::DlgChoise> Gothic::updateDialog(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return game->updateDialog(dlg,player,npc);
  }

void Gothic::dialogExec(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  game->dialogExec(dlg,player,npc);
  }

void Gothic::openDialogPipe(Npc &player, Npc &npc, AiOuputPipe *&pipe) {
  onDialogPipe(player,npc,pipe);
  }

bool Gothic::aiIsDlgFinished() {
  bool v=true;
  isDialogClose(v);
  return v;
  }

const CameraDefinitions& Gothic::getCameraDef() const {
  return *camera;
  }

const Daedalus::GEngineClasses::C_SFX& Gothic::getSoundScheme(const char *name) {
  return soundDef->getSfx(name);
  }

const Daedalus::GEngineClasses::C_MusicTheme& Gothic::getMusicTheme(const char *name) {
  return music->get(name);
  }

const FightAi::FA &Gothic::getFightAi(size_t i) const {
  return fight->get(i);
  }

void Gothic::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  onPrintScreen(msg,x,y,time,font);
  }

void Gothic::print(const char *msg) {
  onPrint(msg);
  }

const std::string &Gothic::messageFromSvm(const std::string &id, int voice) const {
  if(!game){
    static std::string empty;
    return empty;
    }
  return game->messageFromSvm(id,voice);
  }

const std::string &Gothic::messageByName(const std::string &id) const {
  if(!game){
    static std::string empty;
    return empty;
    }
  return game->messageByName(id);
  }

uint32_t Gothic::messageTime(const std::string &id) const {
  if(!game){
    return 0;
    }
  return game->messageTime(id);
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
