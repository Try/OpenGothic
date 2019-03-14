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

  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob);
    }
  loadProgress(55);

  wayNet = std::move(world.waynet);
  adjustWaypoints(wayNet.waypoints);
  adjustWaypoints(freePoints);

  for(auto& i:wayNet.waypoints)
    if(i.wpName.find("START")==0)
      startPoints.push_back(i);

  for(auto& i:wayNet.waypoints)
    if(i.wpName.find("START")!=std::string::npos)
      startPoints.push_back(i);

  std::sort(indexPoints.begin(),indexPoints.end(),[](const ZenLoad::zCWaypointData* a,const ZenLoad::zCWaypointData* b){
    return a->wpName<b->wpName;
    });

  vm->initDialogs(gothic);
  loadProgress(70);

  //const char* hero="PC_HERO";
  const char* hero="PC_ROCKEFELLER";
  //const char* hero="Giant_Bug";

  if(startPoints.size()>0)
    npcPlayer = vm->inserNpc(hero,startPoints[0].wpName.c_str()); else
    npcPlayer = vm->inserNpc(hero,"START");
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

DynamicWorld::Item World::getPhysic(const std::string& visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return physic()->ghostObj(45,mesh->colisionHeight());
  return physic()->ghostObj(45,140);
  }

void World::updateAnimation() {
  static bool doAnim=true;
  if(!vm || !doAnim)
    return;
  for(size_t i=0;i<wobj.npcCount();++i)
    wobj.npc(i).updateAnimation();
  }

void World::tick(uint64_t dt) {
  if(!vm)
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
  switch(pl.weaponState()) {
    case WeaponState::Fist:
    case WeaponState::W1H:
    case WeaponState::W2H:
      fptr=&script()->focusMele();
      break;
    case WeaponState::Bow:
    case WeaponState::CBow:
      fptr=&script()->focusRange();
      break;
    case WeaponState::Mage:
      fptr=&script()->focusMage();
      break;
    case WeaponState::NoWeapon:
      fptr=&script()->focusNorm();
      break;
    }
  auto& policy = *fptr;

  auto n     = policy.npc_prio <0 ? nullptr : wobj.findNpc(pl,v,w,h,         policy.npc_range1,  policy.npc_range2,  policy.npc_azi);
  auto inter = policy.mob_prio <0 ? nullptr : wobj.findInteractive(pl,v,w,h, policy.mob_range1,  policy.mob_range2,  policy.mob_azi);
  auto it    = policy.item_prio<0 ? nullptr : wobj.findItem(pl,v,w,h,        policy.item_range1, policy.item_range2, policy.item_azi);

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

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,
                              int w,int h) const {
  wobj.marchInteractives(p,mvp,w,h);
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

void World::adjustWaypoints(std::vector<ZenLoad::zCWaypointData> &wp) {
  for(auto& w:wp) {
    w.position.y = wdynamic->dropRay(w.position.x,w.position.y,w.position.z);
    for(auto& i:w.wpName)
      i = char(std::toupper(i));
    indexPoints.push_back(&w);
    }
  }

const ZenLoad::zCWaypointData *World::findPoint(const char *name) const {
  if(name==nullptr)
    return nullptr;
  for(auto& i:startPoints)
    if(i.wpName==name)
      return &i;
  auto it = std::lower_bound(indexPoints.begin(),indexPoints.end(),name,[](const ZenLoad::zCWaypointData* a,const char* b){
      return a->wpName<b;
    });
  if(it!=indexPoints.end() && (*it)->wpName==name)
    return *it;
  return nullptr;
  }

const ZenLoad::zCWaypointData *World::findWayPoint(const std::array<float,3> &pos) const {
  return findWayPoint(pos[0],pos[1],pos[2]);
  }

const ZenLoad::zCWaypointData *World::findWayPoint(float x, float y, float z) const {
  const ZenLoad::zCWaypointData* ret =nullptr;
  float                          dist=std::numeric_limits<float>::max();
  for(auto& w:wayNet.waypoints){
    float dx = w.position.x-x;
    float dy = w.position.y-y;
    float dz = w.position.z-z;
    float l=dx*dx+dy*dy+dz*dz;
    if(l<dist){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
  }

const ZenLoad::zCWaypointData *World::findFreePoint(const std::array<float,3> &pos,const char* name) const {
  return findFreePoint(pos[0],pos[1],pos[2],name);
  }

const ZenLoad::zCWaypointData *World::findFreePoint(float x, float y, float z, const char *name) const {
  const ZenLoad::zCWaypointData* ret   = nullptr;
  float                          dist  = 20.f*100.f; // see scripting doc
  auto&                          index = findFpIndex(name);

  for(auto pw:index.index){
    auto& w  = *pw;
    float dx = w.position.x-x;
    float dy = w.position.y-y;
    float dz = w.position.z-z;
    float l=dx*dx+dy*dy+dz*dz;
    if(l<dist){
      ret  = &w;
      dist = l;
      }
    }
  return ret;
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
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    startPoints.push_back(point);
    }
  else if(vob.objectClass=="zCVobSpot:zCVob") {
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    point.position.y = wdynamic->dropRay(vob.position.x,vob.position.y,vob.position.z);
    freePoints.push_back(point);
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

const World::FpIndex &World::findFpIndex(const char *name) const {
  auto it = std::lower_bound(fpIndex.begin(),fpIndex.end(),name,[](FpIndex& l,const char* r){
    return l.key<r;
    });
  if(it!=fpIndex.end() && it->key==name){
    return *it;
    }

  FpIndex id;
  id.key = name;
  for(auto& w:freePoints){
    if(w.wpName.find(name)==std::string::npos)
      continue;
    id.index.push_back(&w);
    }

  it = fpIndex.insert(it,std::move(id));
  return *it;
  }
