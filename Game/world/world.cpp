#include "world.h"

#include <zenload/zCMesh.h>
#include <fstream>
#include <functional>

#include <Tempest/Log>
#include <Tempest/Painter>

#include "gothic.h"
#include "focus.h"
#include "resources.h"
#include "game/serialize.h"
#include "graphics/skeleton.h"

using namespace Tempest;

World::World(GameSession& game,const RendererStorage &storage, std::string file, uint8_t isG2, std::function<void(int)> loadProgress)
  :wname(std::move(file)),game(game),wsound(game,*this),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  loadProgress(1);
  parser.readHeader();

  loadProgress(10);
  ZenLoad::oCWorldData world;
  parser.readWorld(world,isG2==2);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,mesh));
  wview.reset   (new WorldView(*this,mesh,storage));
  loadProgress(70);

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob,true);
    }
  wmatrix->buildIndex();
  loadProgress(100);
  }

World::World(GameSession &game, const RendererStorage &storage,
             Serialize &fin, uint8_t isG2, std::function<void(int)> loadProgress)
  :wname(fin.read<std::string>()),game(game),wsound(game,*this),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  loadProgress(1);
  parser.readHeader();

  loadProgress(10);
  ZenLoad::oCWorldData world;
  parser.readWorld(world,isG2==2);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,mesh));
  wview.reset   (new WorldView(*this,mesh,storage));
  loadProgress(70);

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob,false);
    }
  wmatrix->buildIndex();
  loadProgress(100);
  }

void World::createPlayer(const char *cls) {
  npcPlayer = addNpc(cls,wmatrix->startPoint().name.c_str());
  if(npcPlayer!=nullptr) {
    npcPlayer->setProcessPolicy(Npc::ProcessPolicy::Player);
    game.script()->setInstanceNPC("HERO",*npcPlayer);
    }
  }

void World::insertPlayer(std::unique_ptr<Npc> &&npc,const char* waypoint) {
  if(npc==nullptr)
    return;
  npcPlayer = wobj.insertPlayer(std::move(npc),waypoint);
  if(npcPlayer!=nullptr)
    game.script()->setInstanceNPC("HERO",*npcPlayer);
  }

void World::postInit() {
  // NOTE: level inspector override player stats globaly
  // lvlInspector.reset(new Npc(*this,script().getSymbolIndex("PC_Levelinspektor"),""));

  // game.script()->inserNpc("Snapper",wmatrix->startPoint().name.c_str());
  }

void World::load(Serialize &fout) {
  wobj.load(fout);
  npcPlayer = wobj.findHero();
  if(npcPlayer!=nullptr)
    game.script()->setInstanceNPC("HERO",*npcPlayer);
  }

void World::save(Serialize &fout) {
  fout.write(wname);
  wobj.save(fout);
  }

uint32_t World::npcId(const void *ptr) const {
  return wobj.npcId(ptr);
  }

uint32_t World::itmId(const void *ptr) const {
  return wobj.itmId(ptr);
  }

Npc *World::npcById(uint32_t id) {
  if(id<wobj.npcCount())
    return &wobj.npc(id);
  return nullptr;
  }

Item *World::itmById(uint32_t id) {
  if(id<wobj.itmCount())
    return &wobj.itm(id);
  return nullptr;
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
  if(auto sk=Resources::loadSkeleton(visual))
    return physic()->ghostObj(sk->bboxCol[0],sk->bboxCol[1]);
  ZMath::float3 zero={};
  return physic()->ghostObj(zero,zero);
  }

void World::updateAnimation() {
  static bool doAnim=true;
  if(!doAnim)
    return;
  wobj.updateAnimation();
  }

void World::resetPositionToTA() {
  wobj.resetPositionToTA();
  }

std::unique_ptr<Npc> World::takeHero() {
  return wobj.takeNpc(npcPlayer);
  }

Npc *World::findNpcByInstance(size_t instance) {
  return wobj.findNpcByInstance(instance);
  }

void World::tick(uint64_t dt) {
  static bool doTicks=true;
  if(!doTicks)
    return;
  wobj.tick(dt);
  wdynamic->tick(dt);
  if(auto pl = player())
    wsound.tick(*pl);
  }

uint64_t World::tickCount() const {
  return game.tickCount();
  }

void World::setDayTime(int32_t h, int32_t min) {
  gtime now     = game.time();
  auto  day     = now.day();
  gtime dayTime = now.timeInDay();
  gtime next    = gtime(h,min);

  if(dayTime<=next){
    game.setTime(gtime(day,h,min));
    } else {
    game.setTime(gtime(day+1,h,min));
    }
  wobj.resetPositionToTA();
  }

gtime World::time() const {
  return game.time();
  }

Daedalus::PARSymbol &World::getSymbol(const char *s) const {
  return game.script()->getSymbol(s);
  }

size_t World::getSymbolIndex(const char *s) const {
  return game.script()->getSymbolIndex(s);
  }

Focus World::validateFocus(const Focus &def) {
  Focus ret = def;
  ret.npc         = wobj.validateNpc(ret.npc);
  ret.interactive = wobj.validateInteractive(ret.interactive);
  ret.item        = wobj.validateItem(ret.item);

  return ret;
  }

Focus World::findFocus(const Npc &pl, const Focus& def, const Tempest::Matrix4x4 &v, int w, int h) {
  const Daedalus::GEngineClasses::C_Focus* fptr=&game.script()->focusNorm();
  auto opt  = WorldObjects::NoFlg;
  auto coll = TARGET_COLLECT_FOCUS;

  switch(pl.weaponState()) {
    case WeaponState::Fist:
    case WeaponState::W1H:
    case WeaponState::W2H:
      fptr = &game.script()->focusMele();
      opt  = WorldObjects::NoDeath;
      break;
    case WeaponState::Bow:
    case WeaponState::CBow:
      fptr = &game.script()->focusRange();
      opt  = WorldObjects::NoDeath;
      break;
    case WeaponState::Mage:{
      fptr = &game.script()->focusMage();
      int32_t id  = player()->inventory().activeWeapon()->spellId();
      auto&   spl = script().getSpell(id);

      coll = TargetCollect(spl.targetCollectAlgo);
      opt  = WorldObjects::NoDeath;
      break;
      }
    case WeaponState::NoWeapon:
      fptr = &game.script()->focusNorm();
      break;
    }
  auto& policy = *fptr;

  WorldObjects::SearchOpt optNpc {policy.npc_range1,  policy.npc_range2,  policy.npc_azi,  coll, opt};
  WorldObjects::SearchOpt optMob {policy.mob_range1,  policy.mob_range2,  policy.mob_azi,  coll };
  WorldObjects::SearchOpt optItm {policy.item_range1, policy.item_range2, policy.item_azi, coll };

  auto n     = policy.npc_prio <0 ? nullptr : wobj.findNpc        (pl,def.npc,        v,w,h, optNpc);
  auto inter = policy.mob_prio <0 ? nullptr : wobj.findInteractive(pl,def.interactive,v,w,h, optMob);
  auto it    = policy.item_prio<0 ? nullptr : wobj.findItem       (pl,def.item,       v,w,h, optItm);

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

Focus World::findFocus(const Focus &def, const Tempest::Matrix4x4 &mvp, int w, int h) {
  if(npcPlayer==nullptr)
    return Focus();
  return findFocus(*npcPlayer,def,mvp,w,h);
  }

Trigger *World::findTrigger(const char *name) {
  return wobj.findTrigger(name);
  }

Interactive *World::aviableMob(const Npc &pl, const std::string &name) {
  return wobj.aviableMob(pl,name);
  }

void World::changeWorld(const std::string& world, const std::string& wayPoint) {
  game.changeWorld(world,wayPoint);
  }

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,int w,int h) const {
  wobj.marchInteractives(p,mvp,w,h);
  }

void World::marchPoints(Painter &p, const Matrix4x4 &mvp, int w, int h) const {
  wmatrix->marchPoints(p,mvp,w,h);
  }

AiOuputPipe *World::openDlgOuput(Npc &player, Npc &npc) {
  return game.openDlgOuput(player,npc);
  }

void World::aiOutputSound(Npc &player, const std::string &msg) {
  wsound.aiOutput(player.position(),msg);
  }

bool World::aiIsDlgFinished() {
  return game.aiIsDlgFinished();
  }

void World::printScreen(const char *msg, int x, int y, int time, const Font &font) {
  game.printScreen(msg,x,y,time,font);
  }

void World::print(const char *msg) {
  game.print(msg);
  }

bool World::aiUseMob(Npc &pl, const std::string &name) {
  return wobj.aiUseMob(pl,name);
  }

Npc *World::addNpc(const char *name, const char *at) {
  size_t id = script().getSymbolIndex(name);
  if(id==size_t(-1))
    return nullptr;
  return wobj.addNpc(id,at);
  }

Npc *World::addNpc(size_t npcInstance, const char *at) {
  return wobj.addNpc(npcInstance,at);
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

void World::emitWeaponsSound(Npc &self, Npc &other) {
  /*
   WO - Wood
   ME - Metal
   ST - Stone
   FL - Flesh
   WA - Water
   EA - Earth
   SA - Sand
   UD - Undefined
   */
  // ItemMaterial
  static std::initializer_list<const char*> mat={
    "WO",
    "ST",
    "ME",
    "LE", //MAT_LEATHER,
    "SA"  //MAT_CLAY,
    "ST", //MAT_GLAS,
    };
  auto p0 = self.position();
  auto p1 = other.position();

  const char* selfMt="";
  const char* othMt ="FL";

  if(self.guild()>Guild::GIL_SEPERATOR_HUM)
    selfMt = "JA"; else //Jaws
    selfMt = "FI"; //Fist

  if(auto a = self.inventory().activeWeapon()){
    int32_t m = a->handle()->material;
    if(m==ItemMaterial::MAT_WOOD)
      selfMt = "WO"; else
      selfMt = "ME";
    }

  if(auto a = other.currentArmour()){
    int32_t m = a->handle()->material;
    if(0<=m && size_t(m)<mat.size())
      othMt = *(mat.begin()+m);
    }

  char buf[128]={};
  std::snprintf(buf,sizeof(buf),"CS_MAM_%s_%s",selfMt,othMt);
  wsound.emitSound(buf, 0.5f*(p0[0]+p1[0]), 0.5f*(p0[1]+p1[1]), 0.5f*(p0[2]+p1[2]),25.f,nullptr);
  }

void World::emitBlockSound(Npc &self, Npc &other) {
  // ItemMaterial
  auto p0 = self.position();
  auto p1 = other.position();

  const char* selfMt="ME";
  const char* othMt ="ME";

  if(self.guild()>Guild::GIL_SEPERATOR_HUM)
    selfMt = "JA"; else //Jaws
    selfMt = "FI"; //Fist

  if(auto a = self.inventory().activeWeapon()){
    int32_t m = a->handle()->material;
    if(m==ItemMaterial::MAT_WOOD)
      selfMt = "WO"; else
      selfMt = "ME";
    }

  if(auto a = other.inventory().activeWeapon()){
    int32_t m = a->handle()->material;
    if(m==ItemMaterial::MAT_WOOD)
      selfMt = "WO"; else
      selfMt = "ME";
    }

  char buf[128]={};
  std::snprintf(buf,sizeof(buf),"CS_IAI_%s_%s",selfMt,othMt);
  wsound.emitSound(buf, 0.5f*(p0[0]+p1[0]), 0.5f*(p0[1]+p1[1]), 0.5f*(p0[2]+p1[2]),25.f,nullptr);
  }

bool World::isInListenerRange(const std::array<float,3> &pos) const {
  return wsound.isInListenerRange(pos);
  }

void World::emitDlgSound(const char* s, float x, float y, float z, float range, uint64_t& timeLen) {
  wsound.emitDlgSound(s,x,y,z,range,timeLen);
  }

void World::emitSoundEffect(const char *s, float x, float y, float z, float range, GSoundEffect* slot) {
  wsound.emitSound(s,x,y,z,range,slot);
  }

void World::takeSoundSlot(GSoundEffect &&eff)  {
  wsound.takeSoundSlot(std::move(eff));
  }

void World::tickSlot(GSoundEffect &slot) {
  wsound.tickSlot(slot);
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

const WayPoint *World::findFreePoint(const Npc &npc, const char *name) const {
  if(auto p = npc.currentWayPoint()){
    if(p->isFreePoint() && p->checkName(name)) {
      return p;
      }
    }
  return findFreePoint(npc.position(),name);
  }

const WayPoint *World::findFreePoint(const std::array<float,3> &pos,const char* name) const {
  return findFreePoint(pos[0],pos[1],pos[2],name);
  }

const WayPoint *World::findFreePoint(float x, float y, float z, const char *name) const {
  return wmatrix->findFreePoint(x,y,z,name);
  }

const WayPoint *World::findNextFreePoint(const Npc &n, const char *name) const {
  auto pos = n.position();
  return wmatrix->findNextFreePoint(pos[0],pos[1],pos[2],name,n.currentWayPoint());
  }

const WayPoint *World::findNextPoint(const WayPoint &pos) const {
  return wmatrix->findNextPoint(pos.x,pos.y,pos.z);
  }

void World::detectNpc(const std::array<float,3> p, std::function<void (Npc &)> f) {
  wobj.detectNpc(p[0],p[1],p[2],f);
  }

void World::detectNpc(const float x, const float y, const float z, std::function<void(Npc&)> f) {
  wobj.detectNpc(x,y,z,f);
  }

WayPath World::wayTo(const Npc &pos, const WayPoint &end) const {
  auto p     = pos.position();
  auto point = pos.currentWayPoint();
  if(point && !point->isFreePoint() && MoveAlgo::isClose(pos.position(),*point)){
    return wmatrix->wayTo(*point,end);
    }
  return wmatrix->wayTo(p[0],p[1],p[2],end);
  }

WayPath World::wayTo(float npcX, float npcY, float npcZ, const WayPoint &end) const {
  return wmatrix->wayTo(npcX,npcY,npcZ,end);
  }

GameScript &World::script() const {
  return *game.script();
  }

void World::loadVob(ZenLoad::zCVobData &vob,bool startup) {
  for(auto& i:vob.childVobs)
    loadVob(i,startup);
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
          vob.objectClass=="oCMobBed:oCMobInter:oCMOB:zCVob" ||
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
  else if(vob.objectClass=="oCTriggerScript:zCTrigger:zCVob"){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.objectClass=="zCTriggerList:zCTrigger:zCVob" ||
          vob.objectClass=="zCTrigger:zCVob"){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.objectClass=="zCMessageFilter:zCVob"){
    }
  else if(vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob"){
    }
  else if(vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCVobLensFlare:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCVobStartpoint:zCVob") {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    wmatrix->addStartPoint(vob.position.x,vob.position.y,vob.position.z,dx,dy,dz,vob.vobName.c_str());
    }
  else if(vob.objectClass=="zCVobSpot:zCVob") {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    wmatrix->addFreePoint(vob.position.x,vob.position.y,vob.position.z,dx,dy,dz,vob.vobName.c_str());
    }
  else if(vob.objectClass=="oCItem:zCVob") {
    if(startup)
      addItem(vob);
    }
  else if(vob.objectClass=="zCVobSound" ||
          vob.objectClass=="zCVobSound:zCVob" ||
          vob.objectClass=="zCVobSoundDaytime:zCVobSound:zCVob") {
    addSound(vob);
    }
  else if(vob.objectClass=="oCZoneMusic:zCVob" ||
          vob.objectClass=="oCZoneMusicDefault:oCZoneMusic:zCVob") {
    addMusic(vob);
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

void World::addSound(const ZenLoad::zCVobData &vob) {
  wsound.addSound(vob);
  }

void World::addMusic(const ZenLoad::zCVobData &vob) {
  if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSound)
    wsound.addZone(vob);
  if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusic)
    wsound.addZone(vob);
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusicDefault)
    wsound.setDefaultZone(vob);
  }
