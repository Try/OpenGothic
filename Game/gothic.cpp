#include "gothic.h"

#include <Tempest/Log>
#include <Tempest/TextCodec>

#include <zenload/zCMesh.h>
#include <cstring>
#include <cctype>

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
using namespace FileUtil;

Gothic::Gothic(const int argc, const char **argv) {
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
    else if(std::strcmp(argv[i],"-nofrate")==0){
      noFrate=true;
      }
    else if(std::strcmp(argv[i],"-rambo")==0){
      isRambo=true;
      }
    else if(std::strcmp(argv[i],"-dx12")==0){
      graphics = GraphicBackend::DirectX12;
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

  baseIniFile.reset(new IniFile(nestedPath({u"system",u"Gothic.ini"},Dir::FT_File)));
  iniFile    .reset(new IniFile(u"Gothic.ini"));

  detectGothicVersion();

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
  onSettingsChanged.bind(this,&Gothic::setupSettings);
  setupSettings();
  }

Gothic::~Gothic() {
  }

Gothic::GraphicBackend Gothic::graphicsApi() const {
  return graphics;
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
  if(state!=LoadState::Finalize && state!=LoadState::FailedLoad && state!=LoadState::FailedSave)
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

  onStartLoading();
  auto g = clearGame().release();
  try{
    auto l = std::thread([this,f,g,one]() noexcept {
      std::unique_ptr<GameSession> game(g);
      std::unique_ptr<GameSession> next;
      auto curState = one;
      auto err = (curState==LoadState::Loading) ? LoadState::FailedLoad : LoadState::FailedSave;
      try {
        next        = f(std::move(game));
        pendingGame = std::move(next);
        loadingFlag.compare_exchange_strong(curState,LoadState::Finalize);
        }
      catch(std::bad_alloc&){
        Tempest::Log::e("loading error: out of memory");
        loadingFlag.compare_exchange_strong(curState,err);
        }
      catch(std::system_error&){
        Tempest::Log::e("loading error: unable to open file");
        loadingFlag.compare_exchange_strong(curState,err);
        }
      catch(std::runtime_error& e){
        Tempest::Log::e("loading error: ",e.what());
        loadingFlag.compare_exchange_strong(curState,err);
        }
      catch(std::bad_function_call&){
        Tempest::Log::e("loading error: bad_function_call");
        loadingFlag.compare_exchange_strong(curState,err);
        }
      catch(...) {
        Tempest::Log::e("loading error");
        loadingFlag.compare_exchange_strong(curState,err);
        }
      if(curState==LoadState::Saving) {
        if(game!=nullptr)
          pendingGame = std::move(game);
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
  if(pendingChapter){
    if(aiIsDlgFinished()) {
      onIntroChapter(chapter);
      pendingChapter=false;
      }
    }

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

const Daedalus::GEngineClasses::C_MusicTheme* Gothic::getMusicDef(const char *clsTheme) const {
  return music->get(clsTheme);
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
  setupVmCommonApi(*vm);
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

void Gothic::settingsSetS(const char* sec, const char* name, const char* val) const {
  iniFile->set(sec,name,val);
  onSettingsChanged();
  }

float Gothic::settingsGetF(const char* sec, const char* name) const {
  if(iniFile->has(sec,name))
    return iniFile->getF(sec,name);
  return baseIniFile->getF(sec,name);
  }

void Gothic::settingsSetF(const char* sec, const char* name, float val) {
  iniFile->set(sec,name,val);
  onSettingsChanged();
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

void Gothic::detectGothicVersion() {
  int score[3]={};

  if(gpath.find(u"Gothic/")!=std::string::npos || gpath.find(u"gothic/")!=std::string::npos)
    score[1]++;
  if(FileUtil::exists(nestedPath({u"_work",u"Data",u"Scripts",u"content",u"CUTSCENE",u"OU.BIN"},Dir::FT_File)))
    score[1]++;

  if(FileUtil::exists(nestedPath({u"_work",u"Data",u"Scripts",u"content",u"CUTSCENE",u"OU.DAT"},Dir::FT_File)))
    score[2]++;
  if(baseIniFile->has("KEYSDEFAULT1"))
    score[2]++;
  if(baseIniFile->has("GAME","PATCHVERSION"))
    score[2]++;
  if(baseIniFile->has("GAME","useGothic1Controls"))
    score[2]++;

  if(score[1]>score[2])
    vinfo.game = 1; else
    vinfo.game = 2;

  if(vinfo.game==2) {
    vinfo.patch = baseIniFile->getI("GAME","PATCHVERSION");
    }
  }

void Gothic::setupSettings() {
  const float soundVolume = settingsGetF("SOUND","soundVolume");
  sndDev.setGlobalVolume(soundVolume);
  }

std::unique_ptr<DocumentMenu::Show>& Gothic::getDocument(int id) {
  if(id>=0 && size_t(id)<documents.size())
    return documents[size_t(id)];
  static std::unique_ptr<DocumentMenu::Show> empty;
  return empty;
  }

std::u16string Gothic::nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const {
  return FileUtil::nestedPath(gpath, name, type);
  }

void Gothic::setupVmCommonApi(Daedalus::DaedalusVM& vm) {
  vm.registerUnsatisfiedLink([](Daedalus::DaedalusVM& vm){ notImplementedRoutine(vm); });

  vm.registerExternalFunction("concatstrings", &Gothic::concatstrings);
  vm.registerExternalFunction("inttostring",   &Gothic::inttostring  );
  vm.registerExternalFunction("floattostring", &Gothic::floattostring);
  vm.registerExternalFunction("inttofloat",    &Gothic::inttofloat   );
  vm.registerExternalFunction("floattoint",    &Gothic::floattoint   );

  vm.registerExternalFunction("hlp_strcmp",    &Gothic::hlp_strcmp   );
  vm.registerExternalFunction("hlp_random",    [this](Daedalus::DaedalusVM& vm){ hlp_random(vm); });

  vm.registerExternalFunction("introducechapter",    [this](Daedalus::DaedalusVM& vm){ introducechapter(vm);     });
  vm.registerExternalFunction("playvideo",           [this](Daedalus::DaedalusVM& vm){ playvideo(vm);            });
  vm.registerExternalFunction("playvideoex",         [this](Daedalus::DaedalusVM& vm){ playvideoex(vm);          });
  vm.registerExternalFunction("printscreen",         [this](Daedalus::DaedalusVM& vm){ printscreen(vm);          });
  vm.registerExternalFunction("ai_printscreen",      [this](Daedalus::DaedalusVM& vm){ ai_printscreen(vm);       });
  vm.registerExternalFunction("printdialog",         [this](Daedalus::DaedalusVM& vm){ printdialog(vm);          });
  vm.registerExternalFunction("print",               [this](Daedalus::DaedalusVM& vm){ print(vm);                });

  vm.registerExternalFunction("doc_create",          [this](Daedalus::DaedalusVM& vm){ doc_create(vm);           });
  vm.registerExternalFunction("doc_createmap",       [this](Daedalus::DaedalusVM& vm){ doc_createmap(vm);        });
  vm.registerExternalFunction("doc_setpage",         [this](Daedalus::DaedalusVM& vm){ doc_setpage(vm);          });
  vm.registerExternalFunction("doc_setpages",        [this](Daedalus::DaedalusVM& vm){ doc_setpages(vm);         });
  vm.registerExternalFunction("doc_setmargins",      [this](Daedalus::DaedalusVM& vm){ doc_setmargins(vm);       });
  vm.registerExternalFunction("doc_printline",       [this](Daedalus::DaedalusVM& vm){ doc_printline(vm);        });
  vm.registerExternalFunction("doc_printlines",      [this](Daedalus::DaedalusVM& vm){ doc_printlines(vm);       });
  vm.registerExternalFunction("doc_setfont",         [this](Daedalus::DaedalusVM& vm){ doc_setfont(vm);          });
  vm.registerExternalFunction("doc_setlevel",        [this](Daedalus::DaedalusVM& vm){ doc_setlevel(vm);         });
  vm.registerExternalFunction("doc_setlevelcoords",  [this](Daedalus::DaedalusVM& vm){ doc_setlevelcoords(vm);   });
  vm.registerExternalFunction("doc_show",            [this](Daedalus::DaedalusVM& vm){ doc_show(vm);             });

  vm.registerExternalFunction("exitgame",            [this](Daedalus::DaedalusVM& vm){ exitgame(vm);             });

  vm.registerExternalFunction("printdebug",          [this](Daedalus::DaedalusVM& vm){ printdebug(vm);           });
  vm.registerExternalFunction("printdebugch",        [this](Daedalus::DaedalusVM& vm){ printdebugch(vm);         });
  vm.registerExternalFunction("printdebuginst",      [this](Daedalus::DaedalusVM& vm){ printdebuginst(vm);       });
  vm.registerExternalFunction("printdebuginstch",    [this](Daedalus::DaedalusVM& vm){ printdebuginstch(vm);     });
  }

void Gothic::notImplementedRoutine(Daedalus::DaedalusVM& vm) {
  static std::set<std::string> s;
  auto& fn = vm.currentCall();

  if(s.find(fn)==s.end()){
    s.insert(fn);
    Log::e("not implemented call [",fn,"]");
    }
  }

void Gothic::concatstrings(Daedalus::DaedalusVM &vm) {
  Daedalus::ZString s2 = vm.popString();
  Daedalus::ZString s1 = vm.popString();

  vm.setReturn(s1 + s2);
  }

void Gothic::inttostring(Daedalus::DaedalusVM &vm){
  int32_t x = vm.popInt();
  vm.setReturn(Daedalus::ZString::toStr(x));
  }

void Gothic::floattostring(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloat();
  vm.setReturn(Daedalus::ZString::toStr(x));
  }

void Gothic::floattoint(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloat();
  vm.setReturn(int32_t(x));
  }

void Gothic::inttofloat(Daedalus::DaedalusVM &vm) {
  auto x = vm.popInt();
  vm.setReturn(float(x));
  }

void Gothic::hlp_random(Daedalus::DaedalusVM &vm) {
  uint32_t mod = uint32_t(std::max(1,vm.popInt()));
  vm.setReturn(int32_t(randGen() % mod));
  }

void Gothic::hlp_strcmp(Daedalus::DaedalusVM &vm) {
  const Daedalus::ZString& s2 = vm.popString();
  const Daedalus::ZString& s1 = vm.popString();
  vm.setReturn(s1 == s2 ? 1 : 0);
  }

void Gothic::introducechapter(Daedalus::DaedalusVM &vm) {
  pendingChapter = true;
  ChapterScreen::Show& s = chapter;
  s.time     = vm.popInt();
  s.sound    = vm.popString().c_str();
  s.img      = vm.popString().c_str();
  s.subtitle = vm.popString().c_str();
  s.title    = vm.popString().c_str();
  }

void Gothic::playvideo(Daedalus::DaedalusVM &vm) {
  Daedalus::ZString filename = vm.popString();
  onVideo(filename);
  vm.setReturn(1);
  }

void Gothic::playvideoex(Daedalus::DaedalusVM &vm) {
  int exitSession = vm.popInt();
  int screenBlend = vm.popInt();

  (void)exitSession; // TODO: ex-fetures
  (void)screenBlend;

  Daedalus::ZString filename = vm.popString();
  onVideo(filename);
  vm.setReturn(1);
  }

void Gothic::printscreen(Daedalus::DaedalusVM &vm) {
  int32_t                  timesec = vm.popInt();
  const Daedalus::ZString& font    = vm.popString();
  int32_t                  posy    = vm.popInt();
  int32_t                  posx    = vm.popInt();
  const Daedalus::ZString& msg     = vm.popString();
  onPrintScreen(msg.c_str(),posx,posy,timesec,Resources::font(font.c_str()));
  vm.setReturn(0);
  }

void Gothic::ai_printscreen(Daedalus::DaedalusVM& vm) {
  // TODO: print-screen queue
  int32_t                  timesec = vm.popInt();
  const Daedalus::ZString& font    = vm.popString();
  int32_t                  posy    = vm.popInt();
  int32_t                  posx    = vm.popInt();
  const Daedalus::ZString& msg     = vm.popString();
  onPrintScreen(msg.c_str(),posx,posy,timesec,Resources::font(font.c_str()));
  vm.setReturn(0);
  }

void Gothic::printdialog(Daedalus::DaedalusVM &vm) {
  int32_t     timesec  = vm.popInt();
  const auto& font     = vm.popString();
  int32_t     posy     = vm.popInt();
  int32_t     posx     = vm.popInt();
  const auto& msg      = vm.popString();
  int32_t     dialognr = vm.popInt();
  (void)dialognr;
  onPrintScreen(msg.c_str(),posx,posy,timesec,Resources::font(font.c_str()));
  vm.setReturn(0);
  }

void Gothic::print(Daedalus::DaedalusVM &vm) {
  const auto& msg = vm.popString();
  onPrint(msg.c_str());
  }

void Gothic::doc_create(Daedalus::DaedalusVM &vm) {
  for(size_t i=0;i<documents.size();++i){
    if(documents[i]==nullptr){
      documents[i].reset(new DocumentMenu::Show());
      vm.setReturn(int(i));
      }
    }
  documents.emplace_back(new DocumentMenu::Show());
  vm.setReturn(int(documents.size())-1);
  }

void Gothic::doc_createmap(Daedalus::DaedalusVM &vm) {
  for(size_t i=0;i<documents.size();++i){
    if(documents[i]==nullptr){
      documents[i].reset(new DocumentMenu::Show());
      vm.setReturn(int(i));
      }
    }
  documents.emplace_back(new DocumentMenu::Show());
  vm.setReturn(int(documents.size())-1);
  }

void Gothic::doc_setpage(Daedalus::DaedalusVM &vm) {
  int   scale  = vm.popInt();
  auto  img    = vm.popString();
  int   page   = vm.popInt();
  int   handle = vm.popInt();

  //TODO: scale
  (void)scale;

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.img = img.c_str();
    pg.flg = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Backgr);
    } else {
    doc->img = img.c_str();
    }
  }

void Gothic::doc_setpages(Daedalus::DaedalusVM &vm) {
  int   count  = vm.popInt();
  int   handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc!=nullptr && count>=0 && count<1024){
    doc->pages.resize(size_t(count));
    }
  }

void Gothic::doc_printline(Daedalus::DaedalusVM &vm) {
  auto text   = vm.popString();
  int  page   = vm.popInt();
  int  handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text.c_str();
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_printlines(Daedalus::DaedalusVM &vm) {
  auto text   = vm.popString();
  int  page   = vm.popInt();
  int  handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text.c_str();
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_setmargins(Daedalus::DaedalusVM &vm) {
  int   mul    = vm.popInt();
  int   bottom = vm.popInt() * mul;
  int   right  = vm.popInt() * mul;
  int   top    = vm.popInt() * mul;
  int   left   = vm.popInt() * mul;

  int   page   = vm.popInt();
  int   handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.margins = Tempest::Margin(left,right,top,bottom);
    pg.flg     = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Margin);
    } else {
    doc->margins = Tempest::Margin(left,right,top,bottom);
    }
  }

void Gothic::doc_setfont(Daedalus::DaedalusVM &vm) {
  auto font   = vm.popString();
  int  page   = vm.popInt();
  int  handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;

  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.font = font.c_str();
    pg.flg  = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Font);
    } else {
    doc->font = font.c_str();
    }
  }

void Gothic::doc_show(Daedalus::DaedalusVM &vm) {
  const int handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc!=nullptr){
    onShowDocument(*doc);
    doc.reset();
    }

  while(documents.size()>0 && documents.back()==nullptr)
    documents.pop_back();
  }

void Gothic::doc_setlevel(Daedalus::DaedalusVM& vm) {
  const auto level  = vm.popString();
  const int  handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;

  std::string str = level.c_str();
  size_t bg = str.rfind('\\');
  if(bg!=std::string::npos)
    str = str.substr(bg+1);

  for(auto& i:str)
    i = char(std::tolower(i));

  if(auto w = world()) {
    auto& wname = w->name();
    doc->showPlayer = wname==str;
    }
  }

void Gothic::doc_setlevelcoords(Daedalus::DaedalusVM& vm) {
  int   bottom = vm.popInt();
  int   right  = vm.popInt();
  int   top    = vm.popInt();
  int   left   = vm.popInt();

  int   handle = vm.popInt();

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  doc->wbounds = Rect(left,top,right-left,bottom-top);
  }

void Gothic::exitgame(Daedalus::DaedalusVM&) {
  Tempest::SystemApi::exit();
  }

void Gothic::printdebug(Daedalus::DaedalusVM &vm) {
  const auto& msg = vm.popString();
  if(version().game==2)
    Log::d("[zspy]: ",msg.c_str());
  }

void Gothic::printdebugch(Daedalus::DaedalusVM &vm) {
  const auto& msg = vm.popString();
  int         ch  = vm.popInt();
  if(version().game==2)
    Log::d("[zspy,",ch,"]: ",msg.c_str());
  }

void Gothic::printdebuginst(Daedalus::DaedalusVM &vm) {
  const auto& msg = vm.popString();
  if(version().game==2)
    Log::d("[zspy]: ",msg.c_str());
  }

void Gothic::printdebuginstch(Daedalus::DaedalusVM &vm) {
  auto msg = vm.popString();
  int  ch  = vm.popInt();
  if(version().game==2)
    Log::d("[zspy,",ch,"]: ",msg.c_str());
  }
