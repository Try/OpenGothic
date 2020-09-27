#include "gamesession.h"
#include "savegameheader.h"

#include <Tempest/Log>
#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <cctype>

#include "worldstatestorage.h"
#include "serialize.h"
#include "world/world.h"
#include "gothic.h"

using namespace Tempest;

// rate 14.5 to 1
const uint64_t GameSession::multTime=29;
const uint64_t GameSession::divTime =2;

void GameSession::HeroStorage::save(Npc &npc,World& owner) {
  storage.clear();
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  sr.setContext(&owner);

  npc.save(sr);
  }

void GameSession::HeroStorage::putToWorld(World& owner,const std::string& wayPoint) const {
  if(storage.size()==0)
    return;
  Tempest::MemReader rd{storage};
  Serialize          sr{rd};
  sr.setContext(&owner);

  if(auto pl = owner.player()) {
    pl->load(sr);
    if(auto pos = owner.findPoint(wayPoint)) {
      pl->setPosition  (pos->x,pos->y,pos->z);
      pl->setDirection (pos->dirX,pos->dirY,pos->dirZ);
      pl->attachToPoint(pos);
      pl->updateTransform();
      }
    } else {
    auto ptr = std::make_unique<Npc>(owner,sr);
    owner.insertPlayer(std::move(ptr),wayPoint.c_str());
    }
  }


GameSession::GameSession(Gothic &gothic, const RendererStorage &storage, std::string file)
  :gothic(gothic), storage(storage), cam(gothic) {
  gothic.setLoadingProgress(0);
  setTime(gtime(8,0));
  uint8_t ver = gothic.version().game;

  vm.reset(new GameScript(*this));
  setWorld(std::unique_ptr<World>(new World(gothic,*this,storage,std::move(file),ver,[&](int v){
    gothic.setLoadingProgress(int(v*0.55));
    })));

  vm->initDialogs(gothic);
  gothic.setLoadingProgress(70);

  const bool testMode=false;

  const char* hero = testMode ? "PC_ROCKEFELLER" : "PC_HERO";
  //const char* hero = "PC_ROCKEFELLER";
  //const char* hero = "Sheep";
  //const char* hero = "Giant_Bug";
  //const char* hero = "OrcWarrior_Rest";
  //const char* hero = "Snapper";
  //const char* hero = "Lurker";
  //const char* hero = "Scavenger";
  //const char* hero = "StoneGolem";
  //const char* hero = "Waran";
  //const char* hero = "Bloodfly";
  //const char* hero = "Gobbo_Skeleton";
  wrld->createPlayer(hero);
  wrld->postInit();

  if(!testMode)
    initScripts(true);
  wrld->triggerOnStart(true);
  cam.reset();
  gothic.setLoadingProgress(96);
  }

GameSession::GameSession(Gothic &gothic, const RendererStorage &storage, Serialize &fin)
  :gothic(gothic), storage(storage), cam(gothic) {
  gothic.setLoadingProgress(0);
  uint16_t wssSize=0;

  SaveGameHeader hdr;
  fin.read(hdr,ticks,wrldTimePart);
  wrldTime = hdr.wrldTime;

  fin.read(wssSize);
  for(size_t i=0;i<wssSize;++i)
    visitedWorlds.emplace_back(fin);

  vm.reset(new GameScript(*this,fin));
  setWorld(std::unique_ptr<World>(new World(gothic,*this,storage,fin,hdr.isGothic2,[&](int v){
    gothic.setLoadingProgress(int(v*0.55));
    })));

  vm->initDialogs(gothic);
  gothic.setLoadingProgress(70);
  wrld->load(fin);
  vm->loadVar(fin);
  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);
  cam.load(fin,wrld->player());
  gothic.setLoadingProgress(96);
  }

GameSession::~GameSession() {
  }

void GameSession::save(Serialize &fout, const char* name, const Pixmap& screen) {
  SaveGameHeader hdr;
  hdr.name      = name;
  hdr.priview   = screen;
  hdr.world     = wrld->name();
  hdr.pcTime    = gtime::localtime();
  hdr.wrldTime  = wrldTime;
  hdr.isGothic2 = gothic.version().game;

  fout.write(hdr,ticks,wrldTimePart);
  fout.write(uint16_t(visitedWorlds.size()));

  gothic.setLoadingProgress(5);
  for(auto& i:visitedWorlds)
    i.save(fout);
  gothic.setLoadingProgress(25);

  vm->save(fout);
  gothic.setLoadingProgress(60);
  if(wrld)
    wrld->save(fout);

  gothic.setLoadingProgress(80);
  vm->saveVar(fout);
  cam.save(fout);
  }

void GameSession::setWorld(std::unique_ptr<World> &&w) {
  if(wrld) {
    if(!isWorldKnown(wrld->name()))
      visitedWorlds.emplace_back(*wrld);
    }
  wrld = std::move(w);
  }

std::unique_ptr<World> GameSession::clearWorld() {
  if(wrld) {
    if(!isWorldKnown(wrld->name())) {
      visitedWorlds.emplace_back(*wrld);
      }
    wrld->view()->resetCmd();
    }
  return std::move(wrld);
  }

void GameSession::changeWorld(const std::string& world, const std::string& wayPoint) {
  chWorld.zen = world;
  chWorld.wp  = wayPoint;
  }

void GameSession::exitSession() {
  exitSessionFlg=true;
  }

bool GameSession::isRamboMode() const {
  return gothic.isRamboMode();
  }

const VersionInfo& GameSession::version() const {
  return gothic.version();
  }

WorldView *GameSession::view() const {
  if(wrld)
    return wrld->view();
  return nullptr;
  }

std::vector<uint8_t> GameSession::loadScriptCode() {
  auto path = gothic.nestedPath({u"_work",u"Data",u"Scripts",u"_compiled",u"GOTHIC.DAT"},Dir::FT_File);
  Tempest::RFile f(path);
  std::vector<uint8_t> ret(f.size());
  f.read(ret.data(),ret.size());
  return ret;
  }

void GameSession::setupVmCommonApi(Daedalus::DaedalusVM& vm) {
  gothic.setupVmCommonApi(vm);
  }

SoundFx *GameSession::loadSoundFx(const char *name) {
  return gothic.loadSoundFx(name);
  }

SoundFx *GameSession::loadSoundWavFx(const char *name) {
  return gothic.loadSoundWavFx(name);
  }

const VisualFx* GameSession::loadVisualFx(const char *name) {
  return gothic.loadVisualFx(name);
  }

const ParticleFx* GameSession::loadParticleFx(const char *name) {
  return gothic.loadParticleFx(name);
  }

Tempest::SoundEffect GameSession::loadSound(const Tempest::Sound &raw) {
  return sound.load(raw);
  }

GSoundEffect GameSession::loadSound(const SoundFx &fx) {
  return fx.getEffect(sound);
  }

void GameSession::emitGlobalSound(const Tempest::Sound &sfx) {
  gothic.emitGlobalSound(sfx);
  }

void GameSession::emitGlobalSound(const std::string &sfx) {
  gothic.emitGlobalSound(sfx);
  }

Npc* GameSession::player() {
  if(wrld)
    return wrld->player();
  return nullptr;
  }

void GameSession::updateListenerPos(Npc &npc) {
  auto plPos = npc.position();
  float rot = npc.rotationRad()+float(M_PI/2.0);
  float s   = std::sin(rot);
  float c   = std::cos(rot);
  sound.setListenerPosition(plPos.x,plPos.y+180/*head pos*/,plPos.z);
  sound.setListenerDirection(c,0,s, 0,1,0);
  }

void GameSession::setTime(gtime t) {
  wrldTime = t;
  }

void GameSession::tick(uint64_t dt) {
  ticks+=dt;

  uint64_t add = (dt+wrldTimePart)*multTime;
  wrldTimePart=add%divTime;

  wrldTime.addMilis(add/divTime);
  wrld->tick(dt);
  // std::this_thread::sleep_for(std::chrono::milliseconds(60));

  if(exitSessionFlg) {
    auto& g = gothic;
    exitSessionFlg = false;
    g.clearGame();
    g.onSessionExit();
    return;
    }

  if(!chWorld.zen.empty()) {
    char buf[128]={};
    for(auto& c:chWorld.zen)
      c = char(std::tolower(c));
    size_t beg = chWorld.zen.rfind('\\');
    size_t end = chWorld.zen.rfind('.');

    std::string wname;
    if(beg!=std::string::npos && end!=std::string::npos)
      wname = chWorld.zen.substr(beg+1,end-beg-1);
    else if(end!=std::string::npos)
      wname = chWorld.zen.substr(0,end); else
      wname = chWorld.zen;

    const char *w = (beg!=std::string::npos) ? (chWorld.zen.c_str()+beg+1) : chWorld.zen.c_str();

    if(Resources::vdfsIndex().hasFile(w)) {
      std::snprintf(buf,sizeof(buf),"LOADING_%s.TGA",wname.c_str());  // format load-screen name, like "LOADING_OLDWORLD.TGA"

      gothic.startLoad(buf,[this](std::unique_ptr<GameSession>&& game){
        auto ret = implChangeWorld(std::move(game),chWorld.zen,chWorld.wp);
        chWorld.zen.clear();
        return ret;
        });
      }
    }
  }

auto GameSession::implChangeWorld(std::unique_ptr<GameSession>&& game,
                                  const std::string& world, const std::string& wayPoint) -> std::unique_ptr<GameSession> {
  const char*   w   = world.c_str();
  size_t        cut = world.rfind('\\');
  if(cut!=std::string::npos)
    w = w+cut+1;

  if(!Resources::vdfsIndex().hasFile(w)) {
    Log::i("World not found[",world,"]");
    return std::move(game);
    }

  HeroStorage hdata;
  if(auto hero = wrld->player())
    hdata.save(*hero,*wrld);
  clearWorld();

  vm->resetVarPointers();

  const uint8_t            ver = gothic.version().game;
  const WorldStateStorage& wss = findStorage(w);

  auto loadProgress = [this](int v){
    gothic.setLoadingProgress(v);
    };

  Tempest::MemReader rd{wss.storage.data(),wss.storage.size()};
  Serialize          fin = wss.isEmpty() ? Serialize::empty() : Serialize{rd};

  std::unique_ptr<World> ret;
  if(wss.isEmpty())
    ret = std::unique_ptr<World>(new World(gothic,*this,storage,w,  ver,loadProgress)); else
    ret = std::unique_ptr<World>(new World(gothic,*this,storage,fin,ver,loadProgress));
  setWorld(std::move(ret));

  if(!wss.isEmpty())
    wrld->load(fin);

  if(1){
    // put hero to world
    hdata.putToWorld(*game->wrld,wayPoint);
    }
  initScripts(wss.isEmpty());
  wrld->triggerOnStart(wss.isEmpty());

  for(auto& i:visitedWorlds)
    if(i.name()==wrld->name()){
      i = std::move(visitedWorlds.back());
      visitedWorlds.pop_back();
      break;
      }

  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);

  Log::i("Done loading world[",world,"]");
  return std::move(game);
  }

const WorldStateStorage& GameSession::findStorage(const std::string &name) {
  for(auto& i:visitedWorlds)
    if(i.name()==name)
      return i;
  static WorldStateStorage wss;
  return wss;
  }

void GameSession::updateAnimation() {
  if(wrld)
    wrld->updateAnimation();
  }

std::vector<GameScript::DlgChoise> GameSession::updateDialog(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->updateDialog(dlg,player,npc);
  }

void GameSession::dialogExec(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->exec(dlg,player,npc);
  }

const Daedalus::ZString& GameSession::messageFromSvm(const Daedalus::ZString& id, int voice) const {
  if(!wrld){
    static Daedalus::ZString empty;
    return empty;
    }
  return vm->messageFromSvm(id,voice);
  }

const Daedalus::ZString& GameSession::messageByName(const Daedalus::ZString& id) const {
  if(!wrld){
    static Daedalus::ZString empty;
    return empty;
    }
  return vm->messageByName(id);
  }

uint32_t GameSession::messageTime(const Daedalus::ZString& id) const {
  if(!wrld)
    return 0;
  return vm->messageTime(id);
  }

AiOuputPipe *GameSession::openDlgOuput(Npc &player, Npc &npc) {
  AiOuputPipe* ret=nullptr;
  gothic.openDialogPipe(player, npc, ret);
  return ret;
  }

bool GameSession::aiIsDlgFinished() {
  return gothic.aiIsDlgFinished();
  }

bool GameSession::isWorldKnown(const std::string &name) const {
  for(auto& i:visitedWorlds)
    if(i.name()==name)
      return true;
  return false;
  }

const FightAi::FA &GameSession::getFightAi(size_t i) const {
  return gothic.getFightAi(i);
  }

void GameSession::initScripts(bool firstTime) {
  auto& wname = wrld->name();
  auto dot    = wname.rfind('.');
  auto name   = (dot==std::string::npos ? wname : wname.substr(0,dot));
  if( firstTime ) {
    std::string startup = "startup_"+name;

    if(vm->hasSymbolName(startup.c_str()))
      vm->runFunction(startup.c_str());
    }

  std::string init = "init_"+name;
  if(vm->hasSymbolName(init.c_str()))
    vm->runFunction(init.c_str());

  wrld->resetPositionToTA();
  }
