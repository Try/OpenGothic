#include "gamesession.h"

#include <Tempest/Log>
#include <Tempest/MemReader>
#include <Tempest/MemWriter>

#include "worldstatestorage.h"
#include "serialize.h"
#include "world/world.h"
#include "gothic.h"

using namespace Tempest;

// rate 14.5 to 1
const uint64_t GameSession::multTime=29;
const uint64_t GameSession::divTime =2;

void GameSession::HeroStorage::save(Npc &npc) {
  storage.clear();
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};

  npc.save(sr);
  }

std::unique_ptr<Npc> GameSession::HeroStorage::load(World& owner) const {
  if(storage.size()==0)
    return nullptr;
  Tempest::MemReader rd{storage};
  Serialize          sr{rd};
  return std::make_unique<Npc>(owner,sr);
  }


GameSession::GameSession(Gothic &gothic, const RendererStorage &storage, std::string file)
  :gothic(gothic), storage(storage) {
  gothic.setLoadingProgress(0);
  setTime(gtime(8,0));
  uint8_t ver = gothic.isGothic2() ? 2 : 1;

  vm.reset(new WorldScript(*this));
  setWorld(std::unique_ptr<World>(new World(*this,storage,file,ver,[&](int v){
    gothic.setLoadingProgress(int(v*0.55));
    })));

  vm->initDialogs(gothic);
  gothic.setLoadingProgress(70);

  const char* hero = "PC_HERO";
  //const char* hero = "PC_ROCKEFELLER";
  //const char* hero = "Giant_Bug";
  //const char* hero = "OrcWarrior_Rest";
  //const char* hero = "Snapper";
  //const char* hero = "Lurker";
  //const char* hero = "Scavenger";
  wrld->createPlayer(hero);
  wrld->postInit();

  initScripts(true);
  gothic.setLoadingProgress(96);
  }

GameSession::GameSession(Gothic &gothic, const RendererStorage &storage, Serialize &fin)
  :gothic(gothic), storage(storage) {
  gothic.setLoadingProgress(0);
  bool     isG2=false;
  uint16_t wssSize=0;
  fin.read(ticks,wrldTimePart,wrldTime,isG2);
  fin.read(wssSize);
  for(size_t i=0;i<wssSize;++i)
    visitedWorlds.emplace_back(fin);

  const uint8_t ver = isG2 ? 2 : 1;

  vm.reset(new WorldScript(*this,fin));
  setWorld(std::unique_ptr<World>(new World(*this,storage,fin,ver,[&](int v){
    gothic.setLoadingProgress(int(v*0.55));
    })));

  vm->initDialogs(gothic);
  gothic.setLoadingProgress(70);
  wrld->load(fin);
  vm->loadVar(fin);
  gothic.setLoadingProgress(96);
  }

GameSession::~GameSession() {
  }

void GameSession::save(Serialize &fout) {
  fout.write(ticks,wrldTimePart,wrldTime,bool(gothic.isGothic2()));
  fout.write(uint16_t(visitedWorlds.size()));
  for(auto& i:visitedWorlds)
    i.save(fout);

  vm->save(fout);
  if(wrld)
    wrld->save(fout);
  vm->saveVar(fout);
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

bool GameSession::isRamboMode() const {
  return gothic.isRamboMode();
  }

bool GameSession::isGothic2() const {
  return gothic.isGothic2();
  }

WorldView *GameSession::view() const {
  if(wrld)
    return wrld->view();
  return nullptr;
  }

std::vector<uint8_t> GameSession::loadScriptCode() {
  Tempest::RFile f(gothic.path()+u"/_work/data/Scripts/_compiled/GOTHIC.DAT");
  std::vector<uint8_t> ret(f.size());
  f.read(ret.data(),ret.size());
  return ret;
  }

SoundFx *GameSession::loadSoundFx(const char *name) {
  return gothic.loadSoundFx(name);
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
  sound.setListenerPosition(plPos[0],plPos[1]+180/*head pos*/,plPos[2]);
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

  if(pendingChapter){
    if(aiIsDlgFinished()) {
      gothic.onIntroChapter(chapter);
      pendingChapter=false;
      }
    }

  if(!chWorld.zen.empty()){
    char buf[128]={};
    for(auto& c:chWorld.zen)
      c = char(std::tolower(c));
    size_t beg = chWorld.zen.rfind('\\');
    size_t end = chWorld.zen.rfind('.');

    std::string wname;
    if(beg!=std::string::npos && end!=std::string::npos)
      wname = chWorld.zen.substr(beg+1,end-beg-1); else
      wname = chWorld.zen;

    std::snprintf(buf,sizeof(buf),"LOADING_%s.TGA",wname.c_str());  // format load-screen name, like "LOADING_OLDWORLD.TGA"

    gothic.startLoading(buf,[this](std::unique_ptr<GameSession>&& game){
      auto ret = implChangeWorld(std::move(game),chWorld.zen,chWorld.wp);
      chWorld.zen.clear();
      return ret;
      });
    }
  }

auto GameSession::implChangeWorld(std::unique_ptr<GameSession>&& game,
                                  const std::string& world, const std::string& wayPoint) -> std::unique_ptr<GameSession> {
  const char*   w   = world.c_str();
  size_t        cut = world.rfind('\\');
  if(cut!=std::string::npos)
    w = w+cut+1;

  auto hero = wrld->takeHero();
  HeroStorage hdata;
  if(hero)
    hdata.save(*hero);
  hero = nullptr;
  clearWorld();

  vm->resetVarPointers();

  const uint8_t            ver = gothic.isGothic2() ? 2 : 1;
  const WorldStateStorage& wss = findStorage(w);

  auto loadProgress = [this](int v){
    gothic.setLoadingProgress(v);
    };

  Tempest::MemReader rd{wss.storage.data(),wss.storage.size()};
  Serialize          fin = wss.isEmpty() ? Serialize::empty() : Serialize{rd};

  std::unique_ptr<World> ret;
  if(wss.isEmpty())
    ret = std::unique_ptr<World>(new World(*this,storage,w,  ver,loadProgress)); else
    ret = std::unique_ptr<World>(new World(*this,storage,fin,ver,loadProgress));
  setWorld(std::move(ret));

  if(!wss.isEmpty())
    wrld->load(fin);

  if(1){
    // put hero to world
    auto ptr = hdata.load(*game->wrld);
    game->wrld->insertPlayer(std::move(ptr),wayPoint.c_str());
    }
  initScripts(wss.isEmpty());

  for(auto& i:visitedWorlds)
    if(i.name()==wrld->name()){
      i = std::move(visitedWorlds.back());
      visitedWorlds.pop_back();
      break;
      }

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

std::vector<WorldScript::DlgChoise> GameSession::updateDialog(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->updateDialog(dlg,player,npc);
  }

void GameSession::dialogExec(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->exec(dlg,player.handle(),npc.handle());
  }

const std::string &GameSession::messageFromSvm(const std::string &id, int voice) const {
  if(!wrld){
    static std::string empty;
    return empty;
    }
  return vm->messageFromSvm(id,voice);
  }

const std::string &GameSession::messageByName(const std::string &id) const {
  if(!wrld){
    static std::string empty;
    return empty;
    }
  return vm->messageByName(id);
  }

uint32_t GameSession::messageTime(const std::string &id) const {
  if(!wrld){
    return 0;
    }
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

void GameSession::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  gothic.printScreen(msg,x,y,time,font);
  }

void GameSession::print(const char *msg) {
  gothic.print(msg);
  }

void GameSession::introChapter(const ChapterScreen::Show &s) {
  pendingChapter = true;
  chapter        = s;
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

const Daedalus::GEngineClasses::C_MusicTheme &GameSession::getMusicTheme(const char *name) const {
  return gothic.getMusicTheme(name);
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
