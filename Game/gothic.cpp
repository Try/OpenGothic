#include "gothic.h"

#include <Tempest/Log>
#include <Tempest/TextCodec>

#include <zenload/zCMesh.h>
#include <cstring>

#include "game/definitions/visualfxdefinitions.h"
#include "game/definitions/sounddefinitions.h"
#include "game/definitions/cameradefinitions.h"
#include "game/definitions/musicdefinitions.h"
#include "game/definitions/fightaidefinitions.h"
#include "game/definitions/particlesdefinitions.h"

#include "game/serialize.h"
#include "utils/installdetect.h"
#include "utils/fileutil.h"
#include "utils/inifile.h"

using namespace Tempest;

Gothic::Gothic(const int argc, const char **argv){
  if(argc<1)
    return;

  for(int i=1;i<argc;++i){
    if(std::strcmp(argv[i],"-g")==0){
      ++i;
      if(i<argc)
        gpath.assign(argv[i],argv[i]+std::strlen(argv[i]));
      }
    else if(std::strcmp(argv[i],"-save")==0){
      ++i;
      if(i<argc){
        if(std::strcmp(argv[i],"q")==0) {
          saveDef = "save_slot_0.sav";
          } else {
          saveDef = std::string("save_slot_")+argv[i]+".sav";
          }
        }
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

  gscript = nestedPath({u"_work",u"Data",u"Scripts",u"_compiled"},Dir::FT_Dir);

  if(!validateGothicPath()) {
    Log::e("invalid gothic path: \"",TextCodec::toUtf8(gpath),"\"");
    throw std::logic_error("gothic not found!"); //TODO: user-friendly message-box
    }

  baseIniFile.reset(new IniFile(gpath+u"system/Gothic.ini"));
  iniFile    .reset(new IniFile(u"Gothic.ini"));

  // check actually for gothic-1, any questionable case is g2notr
  if(gpath.find(u"Gothic/")==std::string::npos && gpath.find(u"gothic/")==std::string::npos)
    vinfo.game = 2; else
    vinfo.game = 1;
  if(vinfo.game==2) {
    vinfo.patch = baseIniFile->getI("GAME","PATCHVERSION");
    }

  fight      .reset(new FightAi(*this));
  camera     .reset(new CameraDefinitions(*this));
  soundDef   .reset(new SoundDefinitions(*this));
  particleDef.reset(new ParticlesDefinitions(*this));
  vfxDef     .reset(new VisualFxDefinitions(*this));
  music      .reset(new MusicDefinitions(*this));
  if(wdef.empty()){
    if(version().game==2)
      wdef = "newworld.zen"; else
      wdef = "world.zen";
    }
  }

Gothic::~Gothic() {
  }

const VersionInfo& Gothic::version() const {
  return vinfo;
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

Camera* Gothic::gameCamera() {
  if(game)
    return &game->camera();
  return nullptr;
  }

const QuestLog* Gothic::questLog() const {
  if(game)
    return &game->script()->questLog();
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

SoundFx *Gothic::loadSoundWavFx(const char* name) {
  auto snd = Resources::loadSoundBuffer(name);

  std::lock_guard<std::mutex> guard(syncSnd);
  auto it=sndWavCache.find(name);
  if(it!=sndWavCache.end())
    return &it->second;

  try {
    auto ret = sndWavCache.emplace(name,SoundFx(*this,std::move(snd)));
    return &ret.first->second;
    }
  catch(...){
    Tempest::Log::e("unable to load soundfx \"",name,"\"");
    return nullptr;
    }
  }

const VisualFx* Gothic::loadVisualFx(const char *name) {
  return vfxDef->get(name);
  }

const ParticleFx* Gothic::loadParticleFx(const char *name) {
  return particleDef->get(name);
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
    saveTex = Texture2d();
    onWorldLoaded();
    return true;
    }
  return false;
  }

void Gothic::startSave(Tempest::Texture2d&& tex,
                       const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  saveTex = std::move(tex);
  implStartLoadSave(nullptr,false,f);
  }

void Gothic::startLoad(const char* banner,
                       const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  implStartLoadSave(banner,true,f);
  }

void Gothic::implStartLoadSave(const char* banner,
                               bool load,
                               const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  loadTex = banner==nullptr ? &saveTex : Resources::loadTexture(banner);
  loadProgress.store(0);

  auto zero=LoadState::Idle;
  auto one =load ? LoadState::Loading : LoadState::Saving;
  if(!loadingFlag.compare_exchange_strong(zero,one)){
    return; // loading already
    }

  auto g = clearGame().release();
  try{
    auto l = std::thread([this,f,g,one](){
      std::unique_ptr<GameSession> game(g);
      auto curState = one;
      try {
        auto next   = f(std::move(game));
        pendingGame = std::move(next);
        loadingFlag.compare_exchange_strong(curState,LoadState::Finalize);
        }
      catch(std::bad_alloc&){
        Tempest::Log::e("loading error: out of memory");
        loadingFlag.compare_exchange_strong(curState,LoadState::Failed);
        }
      catch(std::system_error&){
        Tempest::Log::e("loading error: unable to open file");
        loadingFlag.compare_exchange_strong(curState,LoadState::Failed);
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
  if(game)
    game->tick(dt);
  }

void Gothic::updateAnimation() {
  if(game)
    game->updateAnimation();
  }

void Gothic::quickSave() {
  save("save_slot_0.sav");
  }

void Gothic::quickLoad() {
  load("save_slot_0.sav");
  }

void Gothic::save(const std::string &slot) {
  onSaveGame(slot);
  }

void Gothic::load(const std::string &slot) {
  onLoadGame(slot);
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

void Gothic::setMusic(const char *clsTheme) {
  auto& theme = music->get(clsTheme);
  globalMusic.setMusic(theme,clsTheme);
  }

void Gothic::setMusic(const GameMusic::Music m) {
  const char* clsTheme="";
  switch(m) {
    case GameMusic::SysMenu:
      clsTheme = "SYS_Menu";
      break;
    case GameMusic::SysLoading:
      clsTheme = "SYS_Loading";
      break;
    }
  setMusic(clsTheme);
  }

void Gothic::stopMusic() {
  globalMusic.stopMusic();
  }

void Gothic::enableMusic(bool e) {
  globalMusic.setEnabled(e);
  }

bool Gothic::isMusicEnabled() const {
  return globalMusic.isEnabled();
  }

const CameraDefinitions& Gothic::getCameraDef() const {
  return *camera;
  }

const Daedalus::GEngineClasses::C_SFX& Gothic::getSoundScheme(const char *name) {
  return soundDef->getSfx(name);
  }

const FightAi::FA &Gothic::getFightAi(size_t i) const {
  return fight->get(i);
  }

void Gothic::printScreen(const char *msg, int x, int y, int time, const GthFont &font) {
  onPrintScreen(msg,x,y,time,font);
  }

void Gothic::print(const char *msg) {
  onPrint(msg);
  }

const Daedalus::ZString &Gothic::messageFromSvm(const Daedalus::ZString &id, int voice) const {
  if(!game){
    static Daedalus::ZString empty;
    return empty;
    }
  return game->messageFromSvm(id,voice);
  }

const Daedalus::ZString &Gothic::messageByName(const Daedalus::ZString& id) const {
  if(!game){
    static Daedalus::ZString empty;
    return empty;
    }
  return game->messageByName(id);
  }

uint32_t Gothic::messageTime(const Daedalus::ZString& id) const {
  if(!game){
    return 0;
    }
  return game->messageTime(id);
  }

const std::string &Gothic::defaultWorld() const {
  return wdef;
  }

const std::string &Gothic::defaultSave() const {
  return saveDef;
  }

std::unique_ptr<Daedalus::DaedalusVM> Gothic::createVm(const char16_t *datFile) {
  auto path = caseInsensitiveSegment(gscript,datFile,Dir::FT_File);

  Tempest::RFile dat(path);
  size_t all=dat.size();

  std::unique_ptr<uint8_t[]> byte(new uint8_t[all]);
  dat.read(byte.get(),all);

  auto vm = std::make_unique<Daedalus::DaedalusVM>(byte.get(),all);
  Daedalus::registerGothicEngineClasses(*vm);
  return vm;
  }

int Gothic::settingsGetI(const char *sec, const char *name) const {
  if(iniFile->has(sec,name))
    return iniFile->getI(sec,name);
  return baseIniFile->getI(sec,name);
  }

void Gothic::settingsSetI(const char *sec, const char *name, int val) {
  iniFile->set(sec,name,val);
  onSettingsChanged();
  }

const std::string& Gothic::settingsGetS(const char* sec, const char* name) const {
  if(iniFile->has(sec,name))
    return iniFile->getS(sec,name);
  return baseIniFile->getS(sec,name);
  }

void Gothic::flushSettings() const {
  iniFile->flush();
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

bool Gothic::validateGothicPath() const {
  if(gpath.empty())
    return false;
  if(!FileUtil::exists(gscript))
    return false;
  if(!FileUtil::exists(nestedPath({u"Data"},Dir::FT_Dir)))
    return false;
  if(!FileUtil::exists(nestedPath({u"_work",u"Data"},Dir::FT_Dir)))
    return false;
  return true;
  }

std::u16string Gothic::caseInsensitiveSegment(const std::u16string& path,const char16_t* segment,Dir::FileType type) {
  std::u16string next=path+segment;
  if(FileUtil::exists(next)) {
    if(type==Dir::FT_File)
      return next;
    return next+u"/";
    }

  Tempest::Dir::scan(path,[&path,&next,&segment,type](const std::u16string& p, Dir::FileType t){
    if(t!=type)
      return;
    for(size_t i=0;;++i) {
      char16_t cs = segment[i];
      char16_t cp = p[i];
      if('A'<=cs && cs<='Z')
        cs = char16_t(cs-'A'+'a');
      if('A'<=cp && cp<='Z')
        cp = char16_t(cp-'A'+'a');

      if(cs!=cp)
        return;
      if(cs=='\0')
        break;
      }

    next = path+p;
    });
  if(type==Dir::FT_File)
    return next;
  return next+u"/";
  }

std::u16string Gothic::nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const {
  auto path = gpath;
  for(auto& segment:name)
    path = caseInsensitiveSegment(path,segment, (segment==*(name.end()-1)) ? type : Dir::FT_Dir);
  return path;
  }
