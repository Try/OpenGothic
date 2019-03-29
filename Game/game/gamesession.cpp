#include "gamesession.h"

#include "world/world.h"
#include "gothic.h"

// rate 14.5 to 1
const uint64_t GameSession::multTime=29;
const uint64_t GameSession::divTime =2;

GameSession::GameSession(Gothic &gothic, const RendererStorage &storage, std::string file, std::function<void (int)> loadProgress)
  :gothic(gothic) {
  loadProgress(0);
  setTime(gtime(8,0));
  uint8_t ver = gothic.isGothic2() ? 2 : 1;

  vm.reset(new WorldScript(*this));
  setWorld(std::unique_ptr<World>(new World(*this,storage,file,ver,[&](int v){
    loadProgress(int(v*0.55));
    })));

  vm->initDialogs(gothic);
  loadProgress(70);

  const char* hero="PC_HERO";
  //const char* hero="PC_ROCKEFELLER";
  //const char* hero="Giant_Bug";
  //const char* hero="OrcWarrior_Rest";
  //const char* hero = "Snapper";
  //const char* hero = "Lurker";
  //const char* hero="Scavenger";
  wrld->createPlayer(hero);
  wrld->postInit();

  initScripts(true);
  loadProgress(96);
  }

GameSession::~GameSession() {
  }

void GameSession::setWorld(std::unique_ptr<World> &&w) {
  wrld = std::move(w);
  }

std::unique_ptr<World> GameSession::clearWorld() {
  if(wrld)
    wrld->view()->resetCmd();
  return std::move(wrld);
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

Npc* GameSession::player() {
  if(wrld)
    return wrld->player();
  return nullptr;
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

  if(pendingChapter){
    if(aiIsDlgFinished()) {
      gothic.onIntroChapter(chapter);
      pendingChapter=false;
      }
    }
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

void GameSession::aiProcessInfos(Npc &player, Npc &npc) {
  gothic.aiProcessInfos(player,npc);
  }

bool GameSession::aiOutput(Npc &player, const char *msg) {
  return gothic.aiOuput(player,msg);
  }

void GameSession::aiForwardOutput(Npc &player, const char *msg) {
  return gothic.aiForwardOutput(player,msg);
  }

bool GameSession::aiCloseDialog() {
  return gothic.aiCloseDialog();
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
