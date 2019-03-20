#include "world.h"

#include "gothic.h"
#include "focus.h"
#include "resources.h"

#include <zenload/zCMesh.h>
#include <fstream>
#include <functional>
#include <daedalus/DaedalusDialogManager.h>

#include <Tempest/Log>
#include <Tempest/Painter>

using namespace Tempest;

World::World(Gothic& gothic,const RendererStorage &storage, std::string file, std::function<void(int)> loadProgress)
  :wname(std::move(file)),gothic(gothic),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  loadProgress(1);
  parser.readHeader();

  loadProgress(5);
  ZenLoad::oCWorldData world;
  parser.readWorld(world,gothic.isGothic2());

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  loadProgress(25);
  vm.reset      (new WorldScript(*this,gothic,"/_work/data/Scripts/_compiled/GOTHIC.DAT"));
  wdynamic.reset(new DynamicWorld(*this,mesh));
  wview.reset   (new WorldView(*this,mesh,storage));
  loadProgress(35);

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob);
    }
  wmatrix->buildIndex();
  loadProgress(55);

  vm->initDialogs(gothic);
  loadProgress(70);

  const char* hero="PC_HERO";
  //const char* hero="PC_ROCKEFELLER";
  //const char* hero="Giant_Bug";

  npcPlayer = vm->inserNpc(hero,wmatrix->startPoint().name.c_str());
  if(npcPlayer!=nullptr) {
    npcPlayer->setAiType(Npc::AiType::Player);
    vm->setInstanceNPC("HERO",*npcPlayer);
    }

  initScripts(true);
  loadProgress(96);
  }

StaticObjects::Mesh World::getView(const std::string &visual) const {
  return getView(visual,0,0,0);
  }

StaticObjects::Mesh World::getView(const std::string &visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return view()->getView(visual,headTex,teetTex,bodyColor);
  }

StaticObjects::Mesh World::getStaticView(const std::string &visual, int32_t tex) const {
  return view()->getStaticView(visual,tex);
  }

DynamicWorld::Item World::getPhysic(const std::string& visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return physic()->ghostObj(45,mesh->colisionHeight());
  return physic()->ghostObj(45,140);
  }

void World::updateAnimation() {
  static bool doAnim=true;
  if(!vm || !doAnim)
    return;
  //wview->updateAnimation(tickCount());
  wobj.updateAnimation();
  }

void World::tick(uint64_t dt) {
  static bool doTicks=true;
  if(!vm || !doTicks)
    return;
  wobj.tick(dt);
  wdynamic->tick(dt);
  }

uint64_t World::tickCount() const {
  return gothic.tickCount();
  }

void World::setDayTime(int32_t h, int32_t min) {
  gtime now     = gothic.time();
  auto  day     = now.day();
  gtime dayTime = now.timeInDay();
  gtime next    = gtime(h,min);

  if(dayTime<=next){
    gothic.setTime(gtime(day,h,min));
    } else {
    gothic.setTime(gtime(day+1,h,min));
    }
  }

gtime World::time() const {
  return gothic.time();
  }

Daedalus::PARSymbol &World::getSymbol(const char *s) const {
  return vm->getSymbol(s);
  }

size_t World::getSymbolIndex(const char *s) const {
  return vm->getSymbolIndex(s);
  }

Focus World::findFocus(const Npc &pl, const Tempest::Matrix4x4 &v, int w, int h) {
  const Daedalus::GEngineClasses::C_Focus* fptr=&script()->focusNorm();  
  auto opt = WorldObjects::NoFlg;

  switch(pl.weaponState()) {
    case WeaponState::Fist:
    case WeaponState::W1H:
    case WeaponState::W2H:
      fptr = &script()->focusMele();
      opt  = WorldObjects::NoDeath;
      break;
    case WeaponState::Bow:
    case WeaponState::CBow:
      fptr = &script()->focusRange();
      opt  = WorldObjects::NoDeath;
      break;
    case WeaponState::Mage:
      fptr = &script()->focusMage();
      opt  = WorldObjects::NoDeath;
      break;
    case WeaponState::NoWeapon:
      fptr = &script()->focusNorm();
      break;
    }
  auto& policy = *fptr;

  WorldObjects::SearchOpt optNpc {policy.npc_range1,  policy.npc_range2,  policy.npc_azi, opt};
  WorldObjects::SearchOpt optMob {policy.mob_range1,  policy.mob_range2,  policy.mob_azi };
  WorldObjects::SearchOpt optItm {policy.item_range1, policy.item_range2, policy.item_azi};

  auto n     = policy.npc_prio <0 ? nullptr : wobj.findNpc(pl,v,w,h,         optNpc);
  auto inter = policy.mob_prio <0 ? nullptr : wobj.findInteractive(pl,v,w,h, optMob);
  auto it    = policy.item_prio<0 ? nullptr : wobj.findItem(pl,v,w,h,        optItm);

  if(policy.npc_prio>=policy.item_prio &&
     policy.npc_prio>=policy.mob_prio) {
    if(n)
      return Focus(*n);
    if(policy.item_prio>=policy.mob_prio && it)
      return Focus(*it);
    return inter ? Focus(*inter) : Focus();
    }

  if(policy.mob_prio>=policy.item_prio &&
     policy.mob_prio>=policy.npc_prio) {
    if(inter)
      return Focus(*inter);
    if(policy.npc_prio>=policy.item_prio && n)
      return Focus(*n);
    return it ? Focus(*it) : Focus();
    }

  if(policy.item_prio>=policy.mob_prio &&
     policy.item_prio>=policy.npc_prio) {
    if(it)
      return Focus(*it);
    if(policy.npc_prio>=policy.mob_prio && n)
      return Focus(*n);
    return inter ? Focus(*inter) : Focus();
    }

  return Focus();
  }

Focus World::findFocus(const Tempest::Matrix4x4 &mvp, int w, int h) {
  if(npcPlayer==nullptr)
    return Focus();
  return findFocus(*npcPlayer,mvp,w,h);
  }

Trigger *World::findTrigger(const char *name) {
  return wobj.findTrigger(name);
  }

Interactive *World::aviableMob(const Npc &pl, const std::string &name) {
  return wobj.aviableMob(pl,name);
  }

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,int w,int h) const {
  wobj.marchInteractives(p,mvp,w,h);
  }

void World::marchPoints(Painter &p, const Matrix4x4 &mvp, int w, int h) const {
  wmatrix->marchPoints(p,mvp,w,h);
  }

std::vector<WorldScript::DlgChoise> World::updateDialog(const WorldScript::DlgChoise &dlg, Npc& player, Npc& npc) {
  return vm->updateDialog(dlg,player,npc);
  }

void World::exec(const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) {
  return vm->exec(dlg,player.handle(),npc.handle());
  }

void World::aiProcessInfos(Npc &player, Npc &npc) {
  gothic.aiProcessInfos(player,npc);
  }

bool World::aiOutput(Npc &player, const char *msg) {
  return gothic.aiOuput(player,msg);
  }

void World::aiForwardOutput(Npc &player, const char *msg) {
  return gothic.aiForwardOutput(player,msg);
  }

bool World::aiCloseDialog() {
  return gothic.aiCloseDialog();
  }

bool World::aiIsDlgFinished() {
  return gothic.aiIsDlgFinished();
  }

bool World::aiUseMob(Npc &pl, const std::string &name) {
  return wobj.aiUseMob(pl,name);
  }

void World::printScreen(const char *msg, int x, int y, int time, const Font &font) {
  gothic.printScreen(msg,x,y,time,font);
  }

void World::print(const char *msg) {
  gothic.print(msg);
  }

void World::onInserNpc(Daedalus::GameState::NpcHandle handle, const std::string &s) {
  return wobj.onInserNpc(handle,s);
  }

Item *World::addItem(size_t itemInstance, const char *at) {
  return wobj.addItem(itemInstance,at);
  }

Item *World::takeItem(Item &it) {
  return wobj.takeItem(it);
  }

void World::removeItem(Item& it) {
  wobj.removeItem(it);
  }

size_t World::hasItems(const std::string &tag, size_t itemCls) {
  return wobj.hasItems(tag,itemCls);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,perc);
  }

const WayPoint *World::findPoint(const char *name) const {
  return wmatrix->findPoint(name);
  }

const WayPoint* World::findWayPoint(const std::array<float,3> &pos) const {
  return findWayPoint(pos[0],pos[1],pos[2]);
  }

const WayPoint* World::findWayPoint(float x, float y, float z) const {
  return wmatrix->findWayPoint(x,y,z);
  }

const WayPoint *World::findFreePoint(const std::array<float,3> &pos,const char* name) const {
  return findFreePoint(pos[0],pos[1],pos[2],name);
  }

const WayPoint *World::findFreePoint(float x, float y, float z, const char *name) const {
  return wmatrix->findFreePoint(x,y,z,name);
  }

const WayPoint *World::findNextFreePoint(const Npc &n, const char *name) const {
  auto pos = n.position();
  return wmatrix->findNextFreePoint(pos[0],pos[1],pos[2],name);
  }

const WayPoint *World::findNextPoint(const WayPoint &pos) const {
  return wmatrix->findNextPoint(pos.x,pos.y,pos.z);
  }

WayPath World::wayTo(const Npc &pos, const WayPoint &end) const {
  auto p = pos.position();
  return wmatrix->wayTo(p[0],p[1],p[2],end);
  }

WayPath World::wayTo(float npcX, float npcY, float npcZ, const WayPoint &end) const {
  return wmatrix->wayTo(npcX,npcY,npcZ,end);
  }

int32_t World::runFunction(const std::string& fname) {
  return vm->runFunction(fname);
  }

void World::initScripts(bool firstTime) {
  auto dot  = wname.rfind('.');
  auto name = (dot==std::string::npos ? wname : wname.substr(0,dot));
  if( firstTime ) {
    std::string startup = "startup_"+name;

    if(vm->hasSymbolName(startup))
      vm->runFunction(startup);
    }

  std::string init = "init_"+name;
  if(vm->hasSymbolName(init))
    vm->runFunction(init);

  wobj.resetPositionToTA();
  }

void World::loadVob(ZenLoad::zCVobData &vob) {
  for(auto& i:vob.childVobs)
    loadVob(i);
  vob.childVobs.clear(); // because of move

  if(vob.objectClass=="zCVob" ||
     vob.objectClass=="oCMOB:zCVob" ||
     vob.objectClass=="zCPFXControler:zCVob" ||
     vob.objectClass=="oCMobFire:oCMobInter:oCMOB:zCVob") {
    addStatic(vob);
    }
  else if(vob.objectClass=="oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobContainer:oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobDoor:oCMobInter:oCMOB:zCVob" ||
          vob.objectClass=="oCMobSwitch:oCMobInter:oCMOB:zCVob"){
    addInteractive(vob);
    }
  else if(vob.objectClass=="zCVobAnimate:zCVob"){ // ork flags
    addStatic(vob); //TODO: morph animation
    }
  else if(vob.objectClass=="zCVobLevelCompo:zCVob"){
    return;
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCMover){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCTriggerChangeLevel){
    wobj.addTrigger(std::move(vob)); // change world trigger
    }
  else if(vob.objectClass=="zCTriggerWorldStart:zCVob"){
    wobj.addTrigger(std::move(vob)); // world start trigger
    }
  else if(vob.objectClass=="oCTriggerScript:zCTrigger:zCVob" ||
          vob.objectClass=="zCTriggerList:zCTrigger:zCVob" ||
          vob.objectClass=="zCTrigger:zCVob"){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob"){
    }
  else if(vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCVobLensFlare:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCVobStartpoint:zCVob") {
    float y = wdynamic->dropRay(vob.position.x,vob.position.y,vob.position.z);
    wmatrix->addStartPoint(vob.position.x,y,vob.position.z,vob.vobName.c_str());
    }
  else if(vob.objectClass=="zCVobSpot:zCVob") {
    float y = wdynamic->dropRay(vob.position.x,vob.position.y,vob.position.z);
    wmatrix->addFreePoint(vob.position.x,y,vob.position.z,vob.vobName.c_str());
    }
  else if(vob.objectClass=="oCItem:zCVob") {
    addItem(vob);
    }
  else if(vob.objectClass=="zCVobSound" ||
          vob.objectClass=="zCVobSound:zCVob" ||
          vob.objectClass=="zCVobSoundDaytime:zCVobSound:zCVob") {
    }
  else if(vob.objectClass=="oCZoneMusic:zCVob" ||
          vob.objectClass=="oCZoneMusicDefault:oCZoneMusic:zCVob") {
    }
  else if(vob.objectClass=="zCVobLight:zCVob") {
    }
  else {
    static std::unordered_set<std::string> cls;
    if(cls.find(vob.objectClass)==cls.end()){
      cls.insert(vob.objectClass);
      Tempest::Log::d("unknown vob class ",vob.objectClass);
      }
    }
  }

void World::addStatic(const ZenLoad::zCVobData &vob) {
  wview->addStatic(vob);
  }

void World::addInteractive(const ZenLoad::zCVobData &vob) {
  wobj.addInteractive(vob);
  }

void World::addItem(const ZenLoad::zCVobData &vob) {
  wobj.addItem(vob);
  }
