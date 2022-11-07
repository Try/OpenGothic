#include "gamesession.h"
#include "savegameheader.h"

#include <Tempest/Log>
#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <cctype>

#include "worldstatestorage.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "sound/soundfx.h"
#include "serialize.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

// rate 14.5 to 1
const uint64_t GameSession::multTime=29;
const uint64_t GameSession::divTime =2;

void GameSession::HeroStorage::save(Npc& npc) {
  storage.clear();
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  sr.setEntry("hero");

  npc.save(sr,0);
  }

void GameSession::HeroStorage::putToWorld(World& owner, std::string_view wayPoint) const {
  if(storage.size()==0)
    return;
  Tempest::MemReader rd{storage};
  Serialize          sr{rd};
  sr.setEntry("hero");

  if(auto pl = owner.player()) {
    pl->load(sr,0);
    if(auto pos = owner.findPoint(wayPoint)) {
      pl->setPosition  (pos->x,pos->y,pos->z);
      pl->setDirection (pos->dirX,pos->dirY,pos->dirZ);
      pl->attachToPoint(pos);
      pl->updateTransform();
      }
    } else {
    auto ptr = std::make_unique<Npc>(owner,-1,wayPoint);
    ptr->load(sr,0);
    owner.insertPlayer(std::move(ptr),wayPoint);
    }
  }


GameSession::GameSession(std::string file) {
  cam.reset(new Camera());

  Gothic::inst().setLoadingProgress(0);
  setTime(gtime(8,0));

  vm.reset(new GameScript(*this));
  setWorld(std::unique_ptr<World>(new World(*this,std::move(file),true,[&](int v){
    Gothic::inst().setLoadingProgress(int(v*0.55));
    })));

  vm->initDialogs();
  Gothic::inst().setLoadingProgress(70);

  const bool testMode=false;

  std::string_view hero = testMode ? "PC_ROCKEFELLER" : Gothic::inst().defaultPlayer();
  //std::string_view hero = "PC_ROCKEFELLER";
  //std::string_view hero = "PC_HERO";
  //std::string_view hero = "FireGolem";
  //std::string_view hero = "Dragon_Undead";
  //std::string_view hero = "Sheep";
  //std::string_view hero = "Giant_Bug";
  //std::string_view hero = "OrcWarrior_Rest";
  //std::string_view hero = "Snapper";
  //std::string_view hero = "Lurker";
  //std::string_view hero = "Scavenger";
  //std::string_view hero = "StoneGolem";
  //std::string_view hero = "Waran";
  //std::string_view hero = "Bloodfly";
  //std::string_view hero = "Gobbo_Skeleton";
  wrld->createPlayer(hero);
  wrld->postInit();

  if(!testMode)
    initScripts(true);
  wrld->triggerOnStart(true);
  cam->reset(wrld->player());
  Gothic::inst().setLoadingProgress(96);
  ticks = 1;
  // wrld->setDayTime(8,0);
  }

GameSession::GameSession(Serialize &fin) {
  Gothic::inst().setLoadingProgress(0);

  SaveGameHeader hdr;
  fin.setEntry("header");
  fin.read(hdr);
  fin.setGlobalVersion(hdr.version);

  {
  uint16_t wssSize=0;
  fin.read(wssSize);
  visitedWorlds.resize(wssSize);
  for(size_t i=0; i<wssSize; ++i)
    fin.read(visitedWorlds[i].name);
  for(size_t i=0; i<wssSize; ++i)
    visitedWorlds[i].load(fin);
  }

  std::string    wname;
  fin.setEntry("game/session");
  fin.read(ticks,wrldTime,wrldTimePart,wname);

  cam.reset(new Camera());
  vm.reset(new GameScript(*this));
  vm->initDialogs();

  if(true) {
    setWorld(std::unique_ptr<World>(new World(*this,wname,false,[&](int v){
      Gothic::inst().setLoadingProgress(int(v*0.55));
      })));
    wrld->load(fin);
    }

  Gothic::inst().setLoadingProgress(70);

  fin.setEntry("game/quests");
  vm->loadQuests(fin);

  fin.setEntry("game/daedalus");
  vm->loadVar(fin);

  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);

  fin.setEntry("game/camera");
  cam->load(fin,wrld->player());
  Gothic::inst().setLoadingProgress(96);
  }

GameSession::~GameSession() {
  }

void GameSession::save(Serialize &fout, std::string_view name, const Pixmap& screen) {
  SaveGameHeader hdr;
  hdr.version   = Serialize::Version::Current;
  hdr.name      = name;
  hdr.world     = wrld->name();
  {
  time_t now = std::time(nullptr);
  tm*    tp  = std::localtime(&now);
  hdr.pcTime = *tp;
  }
  hdr.wrldTime  = wrldTime;
  hdr.playTime  = ticks;
  hdr.isGothic2 = Gothic::inst().version().game;

  fout.setEntry("header");
  fout.write(hdr);
  {
  uint16_t wssSize = uint16_t(visitedWorlds.size());
  fout.write(wssSize);
  for(auto& i:visitedWorlds)
    fout.write(i.name);
  }

  fout.setEntry("priview.png");
  fout.write(screen);

  fout.setEntry("game/session");
  fout.write(ticks,wrldTime,wrldTimePart,wrld->name());

  fout.setEntry("game/camera");
  cam->save(fout);
  Gothic::inst().setLoadingProgress(5);

  for(auto& i:visitedWorlds) {
    fout.setEntry("worlds/",i.name);
    i.save(fout);
    }
  Gothic::inst().setLoadingProgress(25);

  wrld->save(fout);
  Gothic::inst().setLoadingProgress(60);

  fout.setEntry("game/quests");
  vm->saveQuests(fout);

  fout.setEntry("game/daedalus");
  vm->saveVar(fout);
  Gothic::inst().setLoadingProgress(80);
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
    }
  return std::move(wrld);
  }

void GameSession::changeWorld(std::string_view world, std::string_view wayPoint) {
  chWorld.zen = world;
  chWorld.wp  = wayPoint;
  }

void GameSession::exitSession() {
  exitSessionFlg=true;
  }

const VersionInfo& GameSession::version() const {
  return Gothic::inst().version();
  }

WorldView *GameSession::view() const {
  if(wrld)
    return wrld->view();
  return nullptr;
  }

Tempest::SoundEffect GameSession::loadSound(const Tempest::Sound &raw) {
  return sound.load(raw);
  }

Tempest::SoundEffect GameSession::loadSound(const SoundFx &fx, bool& looped) {
  return fx.getEffect(sound,looped);
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
  wrld->scaleTime(dt);
  ticks+=dt;

  uint64_t add = (dt+wrldTimePart)*multTime;
  wrldTimePart=add%divTime;

  wrldTime.addMilis(add/divTime);

  wrld->tick(dt);
  // std::this_thread::sleep_for(std::chrono::milliseconds(60));

  if(exitSessionFlg) {
    exitSessionFlg = false;
    Gothic::inst().clearGame();
    Gothic::inst().onSessionExit();
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

    if(Resources::hasFile(w)) {
      std::snprintf(buf,sizeof(buf),"LOADING_%s.TGA",wname.c_str());  // format load-screen name, like "LOADING_OLDWORLD.TGA"

      Gothic::inst().startLoad(buf,[this](std::unique_ptr<GameSession>&& game){
        auto ret = implChangeWorld(std::move(game),chWorld.zen,chWorld.wp);
        chWorld.zen.clear();
        return ret;
        });
      }
    }
  }

auto GameSession::implChangeWorld(std::unique_ptr<GameSession>&& game,
                                  std::string_view world, std::string_view wayPoint) -> std::unique_ptr<GameSession> {
  size_t           cut = world.rfind('\\');
  std::string_view w   = world;
  if(cut!=std::string::npos)
    w = world.substr(cut+1);

  if(!Resources::hasFile(w)) {
    Log::i("World not found[",world,"]");
    return std::move(game);
    }

  HeroStorage hdata;
  if(auto hero = wrld->player())
    hdata.save(*hero);
  clearWorld();

  vm->resetVarPointers();

  const WorldStateStorage& wss = findStorage(w);

  auto loadProgress = [](int v) {
    Gothic::inst().setLoadingProgress(v);
    };

  std::unique_ptr<World> ret = std::unique_ptr<World>(new World(*this,w,wss.isEmpty(),loadProgress));
  setWorld(std::move(ret));

  if(!wss.isEmpty()) {
    Tempest::MemReader rd {wss.storage.data(),wss.storage.size()};
    Serialize          fin{rd};
    wrld->load(fin);
    }

  if(1) {
    // put hero to world
    hdata.putToWorld(*game->wrld,wayPoint);
    }
  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);

  initScripts(wss.isEmpty());
  wrld->triggerOnStart(wss.isEmpty());

  for(auto& i:visitedWorlds)
    if(i.name==wrld->name()){
      i = std::move(visitedWorlds.back());
      visitedWorlds.pop_back();
      break;
      }

  cam->reset(wrld->player());
  Log::i("Done loading world[",world,"]");
  return std::move(game);
  }

const WorldStateStorage& GameSession::findStorage(std::string_view name) {
  for(auto& i:visitedWorlds)
    if(i.compareName(name))
      return i;
  static WorldStateStorage wss;
  return wss;
  }

void GameSession::updateAnimation(uint64_t dt) {
  if(wrld)
    wrld->updateAnimation(dt);
  }

std::vector<GameScript::DlgChoise> GameSession::updateDialog(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->updateDialog(dlg,player,npc);
  }

void GameSession::dialogExec(const GameScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->exec(dlg,player,npc);
  }

std::string_view GameSession::messageFromSvm(std::string_view id, int voice) const {
  if(!wrld){
    static std::string empty;
    return empty;
    }
  return vm->messageFromSvm(id,voice);
  }

std::string_view GameSession::messageByName(std::string_view id) const {
  if(!wrld){
    return "";
    }
  return vm->messageByName(id);
  }

uint32_t GameSession::messageTime(std::string_view id) const {
  if(!wrld)
    return 0;
  return vm->messageTime(id);
  }

AiOuputPipe *GameSession::openDlgOuput(Npc &player, Npc &npc) {
  AiOuputPipe* ret=nullptr;
  Gothic::inst().openDialogPipe(player, npc, ret);
  return ret;
  }

bool GameSession::aiIsDlgFinished() {
  return Gothic::inst().aiIsDlgFinished();
  }

bool GameSession::isWorldKnown(std::string_view name) const {
  for(auto& i:visitedWorlds)
    if(i.name==name)
      return true;
  return false;
  }

void GameSession::initScripts(bool firstTime) {
  auto wname = wrld->name();
  auto dot   = wname.rfind('.');
  auto name  = (dot==std::string::npos ? wname : wname.substr(0,dot));

  if(vm->hasSymbolName("startup_global"))
    vm->getVm().call_function("startup_global");

  if(vm->hasSymbolName("init_global"))
    vm->getVm().call_function("init_global");

  if(firstTime) {
    std::string startup = std::string("startup_") + std::string(name);
    if(vm->hasSymbolName(startup))
      vm->getVm().call_function(startup);
    }

  std::string init = std::string("init_") + std::string(name);
  if(vm->hasSymbolName(init))
    vm->getVm().call_function(init);

  wrld->resetPositionToTA();
  }
