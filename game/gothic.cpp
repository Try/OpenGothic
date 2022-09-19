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

#ifndef NDEBUG
  setMarvinEnabled(true);
  setFRate(true);
#endif

  noFrate = CommandLine::inst().noFrate;
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

const Daedalus::ZString& Gothic::messageFromSvm(const Daedalus::ZString &id, int voice) const {
  if(!game){
    static Daedalus::ZString empty;
    return empty;
    }
  return game->messageFromSvm(id,voice);
  }

const Daedalus::ZString& Gothic::messageByName(const Daedalus::ZString& id) const {
  if(!game){
    static Daedalus::ZString empty;
    return empty;
    }
  return game->messageByName(id);
  }

uint32_t Gothic::messageTime(const Daedalus::ZString& id) const {
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

std::unique_ptr<Daedalus::DaedalusVM> Gothic::createVm(std::string_view datFile) {
  auto byte = loadScriptCode(datFile);
  auto vm   = std::make_unique<Daedalus::DaedalusVM>(byte.data(),byte.size());
  Daedalus::registerGothicEngineClasses(*vm);
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
  for(size_t i=0;i<mesh.indices.size();i+=3){
    const uint32_t* tri = &mesh.indices[i];
    out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
    }
  }

void Gothic::debug(const ZenLoad::PackedSkeletalMesh &mesh, std::ostream &out) {
  for(auto& i:mesh.vertices) {
    out << "v  " << i.LocalPositions[0].x << " " << i.LocalPositions[0].y << " " << i.LocalPositions[0].z << std::endl;
    out << "vn " << i.Normal.x   << " " << i.Normal.y   << " " << i.Normal.z   << std::endl;
    out << "vt " << i.TexCoord.x << " " << i.TexCoord.y  << std::endl;
    }
  for(size_t i=0;i<mesh.indices.size();i+=3){
    const uint32_t* tri = &mesh.indices[i];
    out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
    }
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
