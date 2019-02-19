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

World::World(Gothic& gothic,const RendererStorage &storage, std::string file)
  :wname(std::move(file)),gothic(&gothic),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  // TODO: update loader
  parser.readHeader();

  // TODO: update loader
  ZenLoad::oCWorldData world;
  parser.readWorld(world);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    if(i.material.alphaFunc==Resources::AdditiveLight)
      continue;

    blocks.emplace_back();
    Block& b = blocks.back();
    if(i.material.alphaFunc==Resources::Transparent)
      b.alpha=true;
    if(i.material.alphaFunc!=Resources::NoAlpha &&
       i.material.alphaFunc!=Resources::Transparent)
      Log::i("unrecognized alpha func: ",i.material.alphaFunc);

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }

  vm.reset(new WorldScript(*this,gothic,"/_work/data/Scripts/_compiled/GOTHIC.DAT"));
  wdynamic.reset(new DynamicWorld(*this,mesh));
  wview.reset(new WorldView(*this,storage));

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){
    return std::tie(a.alpha,a.texture)<std::tie(b.alpha,b.texture);
    });

  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob);
    }

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

  const char* hero="PC_HERO";
  //const char* hero="PC_ROCKEFELLER";

  if(startPoints.size()>0)
    npcPlayer = vm->inserNpc(hero,startPoints[0].wpName.c_str()); else
    npcPlayer = vm->inserNpc(hero,"START");
  if(npcPlayer!=nullptr) {
    npcPlayer->setAiType(Npc::AiType::Player);
    vm->setInstanceNPC("HERO",*npcPlayer);
    }

  initScripts(true);
  }

StaticObjects::Mesh World::getView(const std::string &visual) {
  return getView(visual,0,0,0);
  }

StaticObjects::Mesh World::getView(const std::string &visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) {
  return view()->getView(visual,headTex,teetTex,bodyColor);
  }

DynamicWorld::Item World::getPhysic(const std::string& visual) {
  if(auto mesh=Resources::loadMesh(visual))
    return physic()->ghostObj(30,mesh->colisionHeight());
  return physic()->ghostObj(30,140);
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
  return gothic->tickCount();
  }

void World::setDayTime(int32_t h, int32_t min) {
  gtime now     = gothic->time();
  auto  day     = now.day();
  gtime dayTime = now.timeInDay();
  gtime next    = gtime(h,min);

  if(dayTime<=next){
    gothic->setTime(gtime(day,h,min));
    } else {
    gothic->setTime(gtime(day+1,h,min));
    }
  }

gtime World::time() const {
  return gothic->time();
  }

Daedalus::PARSymbol &World::getSymbol(const char *s) const {
  return vm->getSymbol(s);
  }

size_t World::getSymbolIndex(const char *s) const {
  return vm->getSymbolIndex(s);
  }

Focus World::findFocus(const Npc &pl, const Tempest::Matrix4x4 &v, int w, int h) {
  auto n = wobj.findNpc(pl,v,w,h);
  if(n)
    return Focus(*n);
  auto inter = wobj.findInteractive(pl,v,w,h);
  return inter ? Focus(*inter) : Focus();
  }

Focus World::findFocus(const Tempest::Matrix4x4 &mvp, int w, int h) {
  if(npcPlayer==nullptr)
    return Focus();
  return findFocus(*npcPlayer,mvp,w,h);
  }

const Trigger *World::findTrigger(const char *name) const {
  return wobj.findTrigger(name);
  }

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,
                              int w,int h) const {
  wobj.marchInteractives(p,mvp,w,h);
  }

std::vector<WorldScript::DlgChoise> World::updateDialog(const WorldScript::DlgChoise &dlg) {
  const Daedalus::GEngineClasses::C_Info& info = vm->getGameState().getInfo(dlg.handle);
  std::vector<WorldScript::DlgChoise> ret;
  for(size_t i=0;i<info.subChoices.size();++i){
    WorldScript::DlgChoise ch;
    ch.title    = info.subChoices[i].text;
    ch.scriptFn = info.subChoices[i].functionSym;
    ch.handle   = dlg.handle;
    ch.sort     = int(i);
    ret.push_back(ch);
    }
  return ret;
  }

void World::exec(const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) {
  return vm->exec(dlg,player.handle(),npc.handle());
  }

void World::aiProcessInfos(Npc &player, Npc &npc) {
  gothic->aiProcessInfos(player,npc);
  }

void World::aiOutput(const char *msg) {
  gothic->aiOuput(msg);
  }

void World::aiCloseDialog() {
  gothic->aiCloseDialog();
  }

bool World::aiIsDlgFinished() {
  return gothic->aiIsDlgFinished();
  }

void World::printScreen(const char *msg, int x, int y, int time, const Font &font) {
  gothic->printScreen(msg,x,y,time,font);
  }

void World::print(const char *msg) {
  gothic->print(msg);
  }

void World::onInserNpc(Daedalus::GameState::NpcHandle handle, const std::string &s) {
  return wobj.onInserNpc(handle,s);
  }

Item *World::addItem(size_t itemInstance, const char *at) {
  return wobj.addItem(itemInstance,at);
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

int32_t World::runFunction(const std::string& fname) {
  return vm->runFunction(fname);
  }

void World::initScripts(bool firstTime) {
  auto dot=wname.rfind('.');
  std::string startup = (firstTime ? "startup_" : "init_")+(dot==std::string::npos ? wname : wname.substr(0,dot));

  if(vm->hasSymbolName(startup))
    vm->runFunction(startup);
  }

void World::loadVob(const ZenLoad::zCVobData &vob) {
  for(auto& i:vob.childVobs)
    loadVob(i);

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
  else if(vob.objectClass=="zCMover:zCTrigger:zCVob"){
    addStatic(vob); // castle gate
    }
  else if(vob.objectClass=="oCTriggerChangeLevel:zCTrigger:zCVob"){
    wobj.addTrigger(vob); // change world trigger
    }
  else if(vob.objectClass=="zCTriggerWorldStart:zCVob"){
    wobj.addTrigger(vob); // world start trigger
    }
  else if(vob.objectClass=="oCTriggerScript:zCTrigger:zCVob" ||
          vob.objectClass=="zCTriggerList:zCTrigger:zCVob" ||
          vob.objectClass=="zCTrigger:zCVob"){
    wobj.addTrigger(vob);
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
