#include "gothic.h"

#include <Tempest/Log>
#include <Tempest/TextCodec>

#include <cstring>
#include <cctype>

#include <phoenix/ext/daedalus_classes.hh>

#include "game/definitions/visualfxdefinitions.h"
#include "game/definitions/sounddefinitions.h"
#include "game/definitions/cameradefinitions.h"
#include "game/definitions/musicdefinitions.h"
#include "game/definitions/fightaidefinitions.h"
#include "game/definitions/particlesdefinitions.h"
#include "game/serialize.h"

#include "utils/fileutil.h"
#include "utils/inifile.h"

#include "commandline.h"

using namespace Tempest;
using namespace FileUtil;

Gothic* Gothic::instance = nullptr;

static bool hasMeshShader() {
  const auto& p = Resources::device().properties();
  if(p.meshlets.meshShader)
    return true;
  if(p.meshlets.meshShaderEmulated)
    ;//return true;
  return false;
  }

Gothic::Gothic() {
  instance = this;

  systemPackIniFile.reset(new IniFile(nestedPath({u"system",u"SystemPack.ini"},Dir::FT_File)));
  showFpsCounter = systemPackIniFile->getI("DEBUG","Show_FPS_Counter");
  hideFocus      = systemPackIniFile->getI("PARAMETERS","HideFocus");

#ifndef NDEBUG
  setMarvinEnabled(true);
  setFRate(true);
#endif

  wrldDef = CommandLine::inst().wrldDef;
  if(hasMeshShader())
    isMeshSh = CommandLine::inst().isMeshShading();

  baseIniFile.reset(new IniFile(nestedPath({u"system",u"Gothic.ini"},Dir::FT_File)));
  iniFile    .reset(new IniFile(u"Gothic.ini"));
  {
  defaults.reset(new IniFile());
  defaults->set("GAME", "enableMouse",         1);
  defaults->set("GAME", "mouseSensitivity",    0.5f);
  defaults->set("GAME", "invCatOrder",         "COMBAT,POTION,FOOD,ARMOR,MAGIC,RUNE,DOCS,OTHER,NONE");
  defaults->set("GAME", "invMaxColumns",       5);
  defaults->set("GAME", "animatedWindows",     1);
  defaults->set("GAME", "useGothic1Controls",  0);
  defaults->set("GAME", "highlightMeleeFocus", 0);

  defaults->set("SKY_OUTDOOR", "zSunName",   "unsun5.tga");
  defaults->set("SKY_OUTDOOR", "zSunSize",   200);
  defaults->set("SKY_OUTDOOR", "zSunAlpha",  230);
  defaults->set("SKY_OUTDOOR", "zMoonName",  "moon.tga");
  defaults->set("SKY_OUTDOOR", "zMoonSize",  400);
  defaults->set("SKY_OUTDOOR", "zMoonAlpha", 255);

  defaults->set("RENDERER_D3D", "zFogRadial", 0);

  defaults->set("SOUND", "musicEnabled",  1);
  defaults->set("SOUND", "musicVolume",   0.5f);
  defaults->set("SOUND", "soundVolume",   0.5f);

  defaults->set("ENGINE", "zEnvMappingEnabled", 0);
  defaults->set("ENGINE", "zCloudShadowScale",  0);
  defaults->set("ENGINE", "zWindEnabled",       1);
  defaults->set("ENGINE", "zWindCycleTime",     4);
  defaults->set("ENGINE", "zWindCycleTimeVar",  6);

  defaults->set("KEYS", "keyEnd",         "0100");
  defaults->set("KEYS", "keyHeal",        "2300");
  defaults->set("KEYS", "keyPotion",      "1900");
  defaults->set("KEYS", "keyLockTarget",  "4f00");
  defaults->set("KEYS", "keyParade",      "cf000d02");
  defaults->set("KEYS", "keyActionRight", "d100");
  defaults->set("KEYS", "keyActionLeft",  "d300");
  defaults->set("KEYS", "keyUp",          "c8001100");
  defaults->set("KEYS", "keyDown",        "d0001f00");
  defaults->set("KEYS", "keyLeft",        "cb001000");
  defaults->set("KEYS", "keyRight",       "cd001200");
  defaults->set("KEYS", "keyStrafeLeft",  "d3001e00");
  defaults->set("KEYS", "keyStrafeRight", "d1002000");
  defaults->set("KEYS", "keyAction",      "1d000c02");
  defaults->set("KEYS", "keySlow",        "2a003600");
  defaults->set("KEYS", "keySMove",       "38009d00");
  defaults->set("KEYS", "keyWeapon",      "39000e02");
  defaults->set("KEYS", "keySneak",       "2d00");
  defaults->set("KEYS", "keyLook",        "13005200");
  defaults->set("KEYS", "keyLookFP",      "21005300");
  defaults->set("KEYS", "keyInventory",   "0f000e00");
  defaults->set("KEYS", "keyShowStatus",  "30002e00");
  defaults->set("KEYS", "keyShowLog",     "31002600");
  defaults->set("KEYS", "keyShowMap",     "3200");
  }

  detectGothicVersion();

  std::u16string_view mod = CommandLine::inst().modPath();
  if(!mod.empty()){
    modFile.reset(new IniFile(mod));
    }

  std::vector<std::u16string> modvdfs;
  if(modFile!=nullptr) {
    wrldDef = modFile->getS("SETTINGS","WORLD");
    size_t split = wrldDef.rfind('\\');
    if(split!=std::string::npos)
      wrldDef = wrldDef.substr(split+1);
    plDef = modFile->getS("SETTINGS","PLAYER");

    std::u16string vdf = TextCodec::toUtf16(std::string(modFile->getS("FILES","VDF")));
    for (size_t start = 0, split = 0; split != std::string::npos; start = split+1) {
      split = vdf.find(' ', start);
      std::u16string mod = vdf.substr(start, split-start);
      if (!mod.empty())
        modvdfs.push_back(mod);
      }
    }
  Resources::loadVdfs(modvdfs);

  if(wrldDef.empty()) {
    if(version().game==2)
      wrldDef = "newworld.zen"; else
      wrldDef = "world.zen";
    }

  if(plDef.empty())
    plDef = "PC_HERO";

  onSettingsChanged.bind(this,&Gothic::setupSettings);
  setupSettings();
  }

Gothic::~Gothic() {
  instance = nullptr;
  }

Gothic& Gothic::inst() {
  return *instance;
  }

void Gothic::setupGlobalScripts() {
  fight      .reset(new FightAi());
  camDef     .reset(new CameraDefinitions());
  soundDef   .reset(new SoundDefinitions());
  particleDef.reset(new ParticlesDefinitions());
  vfxDef     .reset(new VisualFxDefinitions());
  music      .reset(new MusicDefinitions());
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

Camera* Gothic::camera() {
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

SoundFx *Gothic::loadSoundFx(std::string_view name) {
  if(name.empty())
    return nullptr;

  auto cname = std::string(name);

  std::lock_guard<std::mutex> guard(syncSnd);
  auto it=sndFxCache.find(cname);
  if(it!=sndFxCache.end())
    return &it->second;

  try {
    auto ret = sndFxCache.emplace(name,SoundFx(name));
    return &ret.first->second;
    }
  catch(...) {
    Tempest::Log::e("unable to load soundfx \"",cname,"\"");
    return nullptr;
    }
  }

SoundFx *Gothic::loadSoundWavFx(std::string_view name) {
  auto snd   = Resources::loadSoundBuffer(name);
  auto cname = std::string(name);

  std::lock_guard<std::mutex> guard(syncSnd);
  auto it=sndWavCache.find(cname);
  if(it!=sndWavCache.end())
    return &it->second;

  try {
    auto ret = sndWavCache.emplace(name,SoundFx(std::move(snd)));
    return &ret.first->second;
    }
  catch(...) {
    Tempest::Log::e("unable to load soundfx \"",cname,"\"");
    return nullptr;
    }
  }

const VisualFx* Gothic::loadVisualFx(std::string_view name) {
  return vfxDef->get(name);
  }

const ParticleFx* Gothic::loadParticleFx(std::string_view name, bool relaxed) {
  return particleDef->get(name,relaxed);
  }

const ParticleFx* Gothic::loadParticleFx(const ParticleFx* base, const VisualFx::Key* key) {
  return particleDef->get(base,key);
  }

void Gothic::emitGlobalSound(std::string_view sfx) {
  emitGlobalSound(loadSoundFx(sfx));
  }

void Gothic::emitGlobalSound(const SoundFx *sfx) {
  if(sfx!=nullptr) {
    bool loop = false;
    auto s = sfx->getEffect(sndDev,loop);
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

void Gothic::emitGlobalSoundWav(std::string_view wav) {
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

 const std::vector<ItmFlags>& Gothic::invCatOrder() {
  return instance->inventoryOrder;
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

bool Gothic::isMarvinEnabled() const {
  return isMarvin;
  }

void Gothic::setMarvinEnabled(bool m) {
  isMarvin = m;
  }

bool Gothic::doRayQuery() const {
  if(!Resources::device().properties().raytracing.rayQuery)
    return false;
  return CommandLine::inst().isRayQuery();
  }

bool Gothic::doMeshShading() const {
  return isMeshSh;
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
  implStartLoadSave("",false,f);
  }

void Gothic::startLoad(std::string_view banner,
                       const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  implStartLoadSave(banner,true,f);
  }

void Gothic::implStartLoadSave(std::string_view banner,
                               bool load,
                               const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f) {
  loadTex = banner.empty() ? &saveTex : Resources::loadTexture(banner);
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
      catch(const std::exception&e) {
        Tempest::Log::e("loading error: ", e.what());
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

void Gothic::updateAnimation(uint64_t dt) {
  if(game)
    game->updateAnimation(dt);
  }

void Gothic::quickSave() {
  save("save_slot_0.sav","Quick save");
  }

void Gothic::quickLoad() {
  load("save_slot_0.sav");
  }

void Gothic::save(std::string_view slot, std::string_view name) {
  onSaveGame(slot,name);
  }

void Gothic::load(std::string_view slot) {
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

const FightAi& Gothic::fai() {
  return *instance->fight;
  }

const SoundDefinitions& Gothic::sfx() {
  return *instance->soundDef;
  }

const MusicDefinitions& Gothic::musicDef() {
  return *instance->music;
  }

const CameraDefinitions& Gothic::cameraDef() {
  return *instance->camDef;
  }

std::string_view Gothic::messageFromSvm(std::string_view id, int voice) const {
  if(!game)
    return "";
  return game->messageFromSvm(id,voice);
  }

std::string_view Gothic::messageByName(std::string_view id) const {
  if(!game)
    return "";
  return game->messageByName(id);
  }

uint32_t Gothic::messageTime(std::string_view id) const {
  if(!game)
    return 0;
  return game->messageTime(id);
  }

std::string_view Gothic::defaultWorld() const {
  return wrldDef;
  }

std::string_view Gothic::defaultPlayer() const {
  return plDef;
  }

std::string_view Gothic::defaultSave() const {
  return CommandLine::inst().defaultSave();
  }

std::unique_ptr<phoenix::vm> Gothic::createPhoenixVm(std::string_view datFile) {
  auto byte = loadPhoenixScriptCode(datFile);
  phoenix::register_all_script_classes(byte);

  auto vm = std::make_unique<phoenix::vm>(std::move(byte), phoenix::execution_flag::vm_allow_null_instance_access);
  setupVmCommonApi(*vm);
  return vm;
  }

std::vector<uint8_t> Gothic::loadScriptCode(std::string_view datFile) {
  if(Resources::hasFile(datFile))
    return Resources::getFileData(datFile);

  auto gscript = CommandLine::inst().scriptPath();
  char16_t str16[256] = {};
  for(size_t i=0; i<datFile.size() && i<255; ++i)
    str16[i] = char16_t(datFile[i]);
  auto path = caseInsensitiveSegment(gscript,str16,Dir::FT_File);
  Tempest::RFile f(path);
  std::vector<uint8_t> ret(f.size());
  f.read(ret.data(),ret.size());
  return ret;
  }

phoenix::script Gothic::loadPhoenixScriptCode(std::string_view datFile) {
  if(Resources::hasFile(datFile)) {
    auto buf = Resources::getFileBuffer(datFile);
    return phoenix::script::parse(buf);
    }

  auto gscript = CommandLine::inst().scriptPath();
  char16_t str16[256] = {};
  for(size_t i=0; i<datFile.size() && i<255; ++i)
    str16[i] = char16_t(datFile[i]);
  auto path = caseInsensitiveSegment(gscript,str16,Dir::FT_File);
  auto buf = phoenix::buffer::mmap(path);
  return phoenix::script::parse(buf);
  }

int Gothic::settingsGetI(std::string_view sec, std::string_view name) {
  if(name.empty())
    return 0;
  if(instance->iniFile->has(sec,name))
    return instance->iniFile->getI(sec,name);
  if(instance->baseIniFile->has(sec,name))
    return instance->baseIniFile->getI(sec,name);
  if(instance->defaults->has(sec,name))
    return instance->defaults->getI(sec,name);
  return 0;
  }

void Gothic::settingsSetI(std::string_view sec, std::string_view name, int val) {
  instance->iniFile->set(sec,name,val);
  instance->onSettingsChanged();
  }

std::string_view Gothic::settingsGetS(std::string_view sec, std::string_view name) {
  if(name.empty())
    return "";
  if(instance->iniFile->has(sec,name))
    return instance->iniFile->getS(sec,name);
  if(instance->baseIniFile->has(sec,name))
    return instance->baseIniFile->getS(sec,name);
  if(instance->defaults->has(sec,name))
    return instance->defaults->getS(sec,name);
  return "";
  }

void Gothic::settingsSetS(std::string_view sec, std::string_view name, std::string_view val) {
  instance->iniFile->set(sec,name,val);
  instance->onSettingsChanged();
  }

float Gothic::settingsGetF(std::string_view sec, std::string_view name) {
  if(name.empty())
    return 0;
  if(instance->iniFile->has(sec,name))
    return instance->iniFile->getF(sec,name);
  if(instance->baseIniFile->has(sec,name))
    return instance->baseIniFile->getF(sec,name);
  if(instance->defaults->has(sec,name))
    return instance->defaults->getF(sec,name);
  return 0;
  }

void Gothic::settingsSetF(std::string_view sec, std::string_view name, float val) {
  instance->iniFile->set(sec,name,val);
  instance->onSettingsChanged();
  }

void Gothic::flushSettings() {
  instance->iniFile->flush();
  }

void Gothic::detectGothicVersion() {
  int score[3]={};

  auto gpath = CommandLine::inst().rootPath();
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

  if(CommandLine::inst().doForceG1())
    vinfo.game = 1;
  else if(CommandLine::inst().doForceG2())
    vinfo.game = 2;

  if(vinfo.game==2) {
    vinfo.patch = baseIniFile->getI("GAME","PATCHVERSION");
    }
  }

void Gothic::setupSettings() {
  if(game!=nullptr)
    game->setupSettings();

  const float soundVolume = settingsGetF("SOUND","soundVolume");
  sndDev.setGlobalVolume(soundVolume);

  auto        ord  = std::string(Gothic::settingsGetS("GAME","invCatOrder"));
  const char* name = ord.c_str();
  for(size_t i=0; i<=ord.size(); ++i) {
    if(i<ord.size() && ord[i]==',')
      ord[i] = '\0';

    if(i==ord.size() || ord[i]=='\0') {
      ItmFlags v = ITM_CAT_NONE;
      if(std::strcmp(name,"COMBAT")==0)
        v = ItmFlags(ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN);
      else if(std::strcmp(name,"POTION")==0)
        v = ITM_CAT_POTION;
      else if(std::strcmp(name,"FOOD")==0)
        v = ITM_CAT_FOOD;
      else if(std::strcmp(name,"ARMOR")==0)
        v = ITM_CAT_ARMOR;
      else if(std::strcmp(name,"MAGIC")==0)
        v = ITM_CAT_MAGIC;
      else if(std::strcmp(name,"RUNE")==0)
        v = ITM_CAT_RUNE;
      else if(std::strcmp(name,"DOCS")==0)
        v = ITM_CAT_DOCS;
      else if(std::strcmp(name,"OTHER")==0)
        v = ITM_CAT_LIGHT;
      else if(std::strcmp(name,"NONE")==0)
        v = ITM_CAT_NONE;
      else
        continue;
      inventoryOrder.push_back(v);
      name = ord.c_str()+i+1;
      }
    }
  }

std::unique_ptr<DocumentMenu::Show>& Gothic::getDocument(int id) {
  if(id>=0 && size_t(id)<documents.size())
    return documents[size_t(id)];
  static std::unique_ptr<DocumentMenu::Show> empty;
  return empty;
  }

std::u16string Gothic::nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const {
  return CommandLine::inst().nestedPath(name,type);
  }

void Gothic::setupVmCommonApi(phoenix::vm &vm) {
  vm.register_default_external([](std::string_view name) { notImplementedRoutine(std::string {name}); });

  vm.register_external("concatstrings", [](std::string_view a, std::string_view b) { return Gothic::concatstrings(a, b);});
  vm.register_external("inttostring",   [](int i)   { return Gothic::inttostring(i);   });
  vm.register_external("floattostring", [](float f) { return Gothic::floattostring(f); });
  vm.register_external("inttofloat",    [](int i)   { return Gothic::inttofloat(i);    });
  vm.register_external("floattoint",    [](float f) { return Gothic::floattoint(f);    });

  vm.register_external("hlp_strcmp",    [](std::string_view a, std::string_view b) { return Gothic::hlp_strcmp(a, b); });
  vm.register_external("hlp_random",    [this](int max) { return hlp_random(max); });


  vm.register_external("introducechapter", [this](std::string_view title, std::string_view subtitle, std::string_view img, std::string_view sound, int time){ introducechapter(title, subtitle, img, sound, time); });
  vm.register_external("playvideo",        [this](std::string_view name){ return playvideo(name); });
  vm.register_external("playvideoex",      [this](std::string_view name, bool a, bool b){ return playvideoex(name, a, b); });
  vm.register_external("printscreen",      [this](std::string_view msg, int posx, int posy, std::string_view font, int timesec){ return printscreen(msg, posx, posy, font, timesec); });
  vm.register_external("printdialog",      [this](int dialognr, std::string_view msg, int posx, int posy, std::string_view font, int timesec){ return printdialog(dialognr, msg, posx, posy, font, timesec); });
  vm.register_external("print",            [this](std::string_view msg){ print(msg); });

  vm.register_external("doc_create",          [this](){ return doc_create(); });
  vm.register_external("doc_createmap",       [this](){ return doc_createmap(); });
  vm.register_external("doc_setpage",         [this](int handle, int page, std::string_view img, int scale){ doc_setpage(handle, page, img, scale); });
  vm.register_external("doc_setpages",        [this](int handle, int count){ doc_setpages(handle, count); });
  vm.register_external("doc_printline",       [this](int handle, int page, std::string_view text){ doc_printline(handle, page, text); });
  vm.register_external("doc_printlines",      [this](int handle, int page, std::string_view text){ doc_printlines(handle, page, text); });
  vm.register_external("doc_setmargins",      [this](int handle, int page, int left, int top, int right, int bottom, int mul){ doc_setmargins(handle, page, left, top, right, bottom, mul); });
  vm.register_external("doc_setfont",         [this](int handle, int page, std::string_view font){ doc_setfont(handle, page, font); });
  vm.register_external("doc_setlevel",        [this](int handle, std::string_view level){ doc_setlevel(handle, level); });
  vm.register_external("doc_setlevelcoords",  [this](int handle, int left, int top, int right, int bottom){ doc_setlevelcoords(handle, left, top, right, bottom); });
  vm.register_external("doc_show",            [this](int handle){ doc_show(handle); });

  vm.register_external("exitgame",            [this](){ exitgame(); });

  vm.register_external("printdebug",          [this](std::string_view msg){ printdebug(msg); });
  vm.register_external("printdebugch",        [this](int ch, std::string_view msg){ printdebugch(ch, msg); });
  vm.register_external("printdebuginst",      [this](std::string_view msg){ printdebuginst(msg); });
  vm.register_external("printdebuginstch",    [this](int ch, std::string_view msg){ printdebuginstch(ch, msg); });
  }

void Gothic::notImplementedRoutine(std::string_view fn) {
  static std::set<std::string, std::less<>> s;

  if(s.find(fn)==s.end()){
    auto [v, _] = s.insert(std::string {fn});
    Log::e("not implemented call [",v->c_str(),"]");
    }
  }

std::string Gothic::concatstrings(std::string_view a, std::string_view b) {
  return std::string {a} + std::string {b};
  }

std::string Gothic::inttostring(int i){
  return std::to_string(i);
  }

std::string Gothic::floattostring(float f) {
  return std::to_string(f);
  }

int Gothic::floattoint(float f) {
  return static_cast<int>(f);
  }

float Gothic::inttofloat(int i) {
  return static_cast<float>(i);
  }

int Gothic::hlp_random(int max) {
  uint32_t mod = std::max(1, max);
  return static_cast<int32_t>(randGen() % mod);
  }

bool Gothic::hlp_strcmp(std::string_view a, std::string_view b) {
  return a == b;
  }

void Gothic::introducechapter(std::string_view title, std::string_view subtitle, std::string_view img, std::string_view sound, int time) {
  pendingChapter = true;
  ChapterScreen::Show& s = chapter;
  s.time     = time;
  s.sound    = sound;
  s.img      = img;
  s.subtitle = subtitle;
  s.title    = title;
  }

bool Gothic::playvideo(std::string_view name) {
  onVideo(name);
  return true;
  }

bool Gothic::playvideoex(std::string_view name, bool, bool) {
  onVideo(name);
  return true;
  }

bool Gothic::printscreen(std::string_view msg, int posx, int posy, std::string_view font, int timesec) {
  onPrintScreen(msg,posx,posy,timesec,Resources::font(font));
  return false;
  }

bool Gothic::printdialog(int, std::string_view msg, int posx, int posy, std::string_view font, int timesec) {
  onPrintScreen(msg,posx,posy,timesec,Resources::font(font));
  return false;
  }

void Gothic::print(std::string_view msg) {
  onPrint(msg);
  }

int Gothic::doc_create() {
  for(size_t i=0;i<documents.size();++i){
    if(documents[i]==nullptr){
      documents[i].reset(new DocumentMenu::Show());
      return static_cast<int>(i);
      }
    }
  documents.emplace_back(new DocumentMenu::Show());
  return static_cast<int>(documents.size()) - 1;
  }

int Gothic::doc_createmap() {
  for(size_t i=0;i<documents.size();++i){
    if(documents[i]==nullptr){
      documents[i].reset(new DocumentMenu::Show());
      return static_cast<int>(i);
      }
    }
  documents.emplace_back(new DocumentMenu::Show());
  return static_cast<int>(documents.size())-1;
  }

void Gothic::doc_setpage(int handle, int page, std::string_view img, int scale) {
  //TODO: scale
  (void)scale;

  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.img = img;
    pg.flg = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Backgr);
    } else {
    doc->img = img;
    }
  }

void Gothic::doc_setpages(int handle, int count) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr && count>=0 && count<1024){
    doc->pages.resize(size_t(count));
    }
  }

void Gothic::doc_printline(int handle, int page, std::string_view text) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_printlines(int handle, int page, std::string_view text) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_setmargins(int handle, int page, int left, int top, int right, int bottom, int mul) {
  bottom *=  mul;
  right  *=  mul;
  top    *=  mul;
  left   *=  mul;

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

void Gothic::doc_setfont(int handle, int page, std::string_view font) {
  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;

  if(page>=0 && size_t(page)<doc->pages.size()){
    auto& pg = doc->pages[size_t(page)];
    pg.font = font;
    pg.flg  = DocumentMenu::Flags(pg.flg | DocumentMenu::F_Font);
    } else {
    doc->font = font;
    }
  }

void Gothic::doc_show(int handle) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr){
    onShowDocument(*doc);
    doc.reset();
    }

  while(documents.size()>0 && documents.back()==nullptr)
    documents.pop_back();
  }

void Gothic::doc_setlevel(int handle, std::string_view level) {
  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;

  std::string str {level};
  size_t bg = str.rfind('\\');
  if(bg!=std::string::npos)
    str = str.substr(bg+1);

  for(auto& i:str)
    i = char(std::tolower(i));

  if(auto w = world()) {
    doc->showPlayer = (w->name()==str);
    }
  }

void Gothic::doc_setlevelcoords(int handle, int left, int top, int right, int bottom) {
  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  doc->wbounds = Rect(left,top,right-left,bottom-top);
  }

void Gothic::exitgame() {
  Tempest::SystemApi::exit();
  }

void Gothic::printdebug(std::string_view msg) {
  if(version().game==2)
    Log::d("[zspy]: ",msg);
  }

void Gothic::printdebugch(int ch, std::string_view msg) {
  if(version().game==2)
    Log::d("[zspy,",ch,"]: ",msg);
  }

void Gothic::printdebuginst(std::string_view msg) {
  if(version().game==2)
    Log::d("[zspy]: ",msg);
  }

void Gothic::printdebuginstch(int ch, std::string_view msg) {
  if(version().game==2)
    Log::d("[zspy,",ch,"]: ",msg);
  }
