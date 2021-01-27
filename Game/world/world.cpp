#include "world.h"

#include <zenload/zCMesh.h>
#include <fstream>
#include <functional>
#include <cctype>

#include <Tempest/Log>
#include <Tempest/Painter>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/visualfx.h"
#include "world/objects/globalfx.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "game/globaleffects.h"
#include "game/serialize.h"
#include "gothic.h"
#include "focus.h"
#include "resources.h"

World::World(Gothic& gothic, GameSession& game,const RendererStorage &storage, std::string file, uint8_t isG2, std::function<void(int)> loadProgress)
  :wname(std::move(file)),game(game),wsound(gothic,game,*this),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  loadProgress(1);
  parser.readHeader();

  loadProgress(10);
  ZenLoad::oCWorldData world;
  parser.readWorld(world,isG2==2);

  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  PackedMesh vmesh(*worldMesh,PackedMesh::PK_VisualLnd);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,*worldMesh));
  wview.reset   (new WorldView(*this,vmesh,storage));
  loadProgress(70);

  globFx.reset(new GlobalEffects(*this));

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      wobj.addRoot(std::move(vob),true);
    }
  wmatrix->buildIndex();
  bsp = std::move(world.bspTree);
  bspSectors.resize(bsp.sectors.size());
  loadProgress(100);
  }

World::World(Gothic& gothic, GameSession &game, const RendererStorage &storage,
             Serialize &fin, uint8_t isG2, std::function<void(int)> loadProgress)
  :wname(fin.read<std::string>()),game(game),wsound(gothic,game,*this),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());

  loadProgress(1);
  parser.readHeader();

  loadProgress(10);
  ZenLoad::oCWorldData world;
  parser.readWorld(world,isG2==2);

  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  PackedMesh vmesh(*worldMesh,PackedMesh::PK_VisualLnd);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,*worldMesh));
  wview.reset   (new WorldView(*this,vmesh,storage));
  loadProgress(70);

  globFx.reset(new GlobalEffects(*this));

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      wobj.addRoot(std::move(vob),false);
    }
  wmatrix->buildIndex();
  bsp = std::move(world.bspTree);
  bspSectors.resize(bsp.sectors.size());

  loadProgress(100);
  }

World::~World() {
  }

void World::createPlayer(const char *cls) {
  npcPlayer = addNpc(cls,wmatrix->startPoint().name);
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

void World::load(Serialize &fin) {
  fin.setContext(this);
  wobj.load(fin);

  uint32_t sz=0;
  fin.read(sz);
  std::string tag;
  for(size_t i=0;i<sz;++i) {
    int32_t guild=GIL_NONE;
    fin.read(tag,guild);
    if(auto p = portalAt(tag))
      p->guild = guild;
    }

  npcPlayer = wobj.findHero();
  }

void World::save(Serialize &fout) {
  fout.setContext(this);
  fout.write(wname);
  wobj.save(fout);

  fout.write(uint32_t(bspSectors.size()));
  for(size_t i=0;i<bspSectors.size();++i) {
    fout.write(bsp.sectors[i].name,bspSectors[i].guild);
    }
  }

uint32_t World::npcId(const Npc *ptr) const {
  return wobj.npcId(ptr);
  }

Npc *World::npcById(uint32_t id) {
  if(id<wobj.npcCount())
    return &wobj.npc(id);
  return nullptr;
  }

uint32_t World::mobsiId(const Interactive* ptr) const {
  return wobj.mobsiId(ptr);
  }

Interactive* World::mobsiById(uint32_t id) {
  if(id<wobj.mobsiCount())
    return &wobj.mobsi(id);
  return nullptr;
  }

uint32_t World::itmId(const void *ptr) const {
  return wobj.itmId(ptr);
  }

Item *World::itmById(uint32_t id) {
  if(id<wobj.itmCount())
    return &wobj.itm(id);
  return nullptr;
  }

void World::runEffect(Effect&& e) {
  wobj.runEffect(std::move(e));
  }

void World::stopEffect(const VisualFx& root) {
  auto* vfx = &root;
  while(vfx!=nullptr) {
    globFx->stopEffect(*vfx);
    vfx = loadVisualFx(vfx->handle().emFXCreate_S.c_str());
    }
  wobj.stopEffect(root);
  }

GlobalFx World::addGlobalEffect(const Daedalus::ZString& what, float len, const Daedalus::ZString* argv, size_t argc) {
  return globFx->startEffect(what,len,argv,argc);
  }

LightGroup::Light World::addLight() {
  return wview->addLight();
  }

LightGroup::Light World::addLight(const ZenLoad::zCVobData& vob) {
  return wview->addLight(vob);
  }

MeshObjects::Mesh World::addView(const char* visual) const {
  return addView(visual,0,0,0);
  }

MeshObjects::Mesh World::addView(const Daedalus::ZString& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return addView(visual.c_str(),headTex,teetTex,bodyColor);
  }

MeshObjects::Mesh World::addView(const char* visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return view()->addView(visual,headTex,teetTex,bodyColor);
  }

PfxEmitter World::addView(const ParticleFx *decl) const {
  return view()->addView(decl);
  }

PfxEmitter World::addView(const ZenLoad::zCVobData& vob) const {
  return view()->addView(vob);
  }

MeshObjects::Mesh World::addAtachView(const ProtoMesh::Attach& visual, const int32_t version) {
  return view()->addAtachView(visual,version);
  }

MeshObjects::Mesh World::addItmView(const Daedalus::ZString& visual, int32_t tex) const {
  return addItmView(visual.c_str(),tex);
  }

MeshObjects::Mesh World::addItmView(const char* visual, int32_t tex) const {
  return view()->addItmView(visual,tex);
  }

MeshObjects::Mesh World::addStaticView(const char* visual) const {
  return view()->addStaticView(visual);
  }

MeshObjects::Mesh World::addDecalView(const ZenLoad::zCVobData& vob) const {
  return view()->addDecalView(vob);
  }

MeshObjects::Mesh World::addView(const Daedalus::ZString& visual) const {
  return addView(visual.c_str());
  }

const VisualFx *World::loadVisualFx(const char *name) {
  return game.loadVisualFx(name);
  }

const ParticleFx* World::loadParticleFx(const char *name) const {
  return game.loadParticleFx(name);
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

const std::string& World::roomAt(const Tempest::Vec3& p) {
  static std::string empty;

  if(bsp.nodes.empty())
    return empty;

  const ZenLoad::zCBspNode* node=&bsp.nodes[0];

  while(true) {
    const float* v    = node->plane.v;
    float        sgn  = v[0]*p.x + v[1]*p.y + v[2]*p.z - v[3];
    uint32_t     next = (sgn>0) ? node->front : node->back;
    if(next>=bsp.nodes.size())
      break;

    node = &bsp.nodes[next];
    }

  if(node->bbox3dMin.x <= p.x && p.x <node->bbox3dMax.x &&
     node->bbox3dMin.y <= p.y && p.y <node->bbox3dMax.y &&
     node->bbox3dMin.z <= p.z && p.z <node->bbox3dMax.z) {
    return roomAt(*node);
    }

  return empty;
  }

const std::string& World::roomAt(const ZenLoad::zCBspNode& node) {
  std::string* ret=nullptr;
  size_t       count=0;
  auto         id = &node-bsp.nodes.data();(void)id;

  for(auto& i:bsp.sectors) {
    for(auto r:i.bspNodeIndices)
      if(r<bsp.leafIndices.size()){
        size_t idx = bsp.leafIndices[r];
        if(idx>=bsp.nodes.size())
          continue;
        if(&bsp.nodes[idx]==&node) {
          ret = &i.name;
          count++;
          }
        }
    }
  if(count==1) {
    // TODO: portals
    return *ret;
    }
  static std::string empty;
  return empty;
  }

World::BspSector* World::portalAt(const std::string &tag) {
  if(tag.empty())
    return nullptr;

  for(size_t i=0;i<bsp.sectors.size();++i)
    if(bsp.sectors[i].name==tag)
      return &bspSectors[i];
  return nullptr;
  }

void World::scaleTime(uint64_t& dt) {
  globFx->scaleTime(dt);
  }

void World::tick(uint64_t dt) {
  static bool doTicks=true;
  if(!doTicks)
    return;
  wobj.tick(dt,dt);
  wdynamic->tick(dt);
  wview->tick(dt);
  if(auto pl = player())
    wsound.tick(*pl);
  globFx->tick(dt);
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

Focus World::findFocus(const Npc &pl, const Focus& def) {
  const Daedalus::GEngineClasses::C_Focus* fptr=&game.script()->focusNorm();
  auto opt    = WorldObjects::NoFlg;
  auto coll   = TARGET_COLLECT_FOCUS;
  auto weapon = pl.inventory().activeWeapon();

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
      if(weapon!=nullptr) {
        int32_t id  = weapon->spellId();
        auto&   spl = script().getSpell(id);
        coll = TargetCollect(spl.targetCollectAlgo);
        }
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

  auto n     = policy.npc_prio <0 ? nullptr : wobj.findNpc        (pl,def.npc,        optNpc);
  auto it    = policy.item_prio<0 ? nullptr : wobj.findItem       (pl,def.item,       optItm);
  auto inter = policy.mob_prio <0 ? nullptr : wobj.findInteractive(pl,def.interactive,optMob);
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    optMob.flags = WorldObjects::SearchFlg(WorldObjects::FcOverride | WorldObjects::NoRay);
    inter = wobj.findInteractive(pl,def.interactive,optMob);
    }

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

Focus World::findFocus(const Focus &def) {
  if(npcPlayer==nullptr)
    return Focus();
  return findFocus(*npcPlayer,def);
  }

Interactive *World::aviableMob(const Npc &pl, const char* name) {
  return wobj.aviableMob(pl,name);
  }

Interactive* World::findInteractive(const Npc& pl) {
  WorldObjects::SearchOpt optMvMob;
  optMvMob.rangeMax = 100;
  optMvMob.flags    = WorldObjects::SearchFlg::NoAngle;

  return wobj.findInteractive(pl,nullptr,optMvMob);
  }

void World::triggerEvent(const TriggerEvent &e) {
  wobj.triggerEvent(e);
  }

void World::execTriggerEvent(const TriggerEvent& e) {
  wobj.execTriggerEvent(e);
  }

void World::enableTicks(AbstractTrigger& t) {
  wobj.enableTicks(t);
  }

void World::disableTicks(AbstractTrigger& t) {
  wobj.disableTicks(t);
  }

void World::triggerChangeWorld(const std::string& world, const std::string& wayPoint) {
  game.changeWorld(world,wayPoint);
  }

void World::setMobRoutine(gtime time, const Daedalus::ZString& scheme, int32_t state) {
  wobj.setMobRoutine(time,scheme,state);
  }

void World::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,int w,int h) const {
  wobj.marchInteractives(p,mvp,w,h);
  }

void World::marchPoints(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
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

bool World::isTargeted(Npc& npc) {
  return wobj.isTargeted(npc);
  }

Npc *World::addNpc(const char *name, const Daedalus::ZString& at) {
  size_t id = script().getSymbolIndex(name);
  if(id==size_t(-1))
    return nullptr;
  return wobj.addNpc(id,at);
  }

Npc *World::addNpc(size_t npcInstance, const Daedalus::ZString& at) {
  return wobj.addNpc(npcInstance,at);
  }

Npc* World::addNpc(size_t itemInstance, const Tempest::Vec3& at) {
  return wobj.addNpc(itemInstance,at);
  }

Item *World::addItem(size_t itemInstance, const char *at) {
  return wobj.addItem(itemInstance,at);
  }

Item* World::addItem(const ZenLoad::zCVobData& vob) {
  return wobj.addItem(vob);
  }

Item *World::takeItem(Item &it) {
  return wobj.takeItem(it);
  }

void World::removeItem(Item& it) {
  wobj.removeItem(it);
  }

size_t World::hasItems(const char* tag, size_t itemCls) {
  return wobj.hasItems(tag,itemCls);
  }

Bullet& World::shootSpell(const Item &itm, const Npc &npc, const Npc *target) {
  float dx  = 1.f, dy = 0.f, dz = 0.f;
  auto  pos = npc.position();
  auto  bn  = npc.mapWeaponBone();
  pos+=bn;

  if(target!=nullptr) {
    auto  tgPos = target->position();
    float y1    = target->centerY();
    float y0    = pos.y;

    dx = tgPos.x-pos.x;
    dy = y1-y0;
    dz = tgPos.z-pos.z;
    } else {
    float a = npc.rotationRad()-float(M_PI/2);
    float c = std::cos(a), s = std::sin(a);
    dx = c;
    dz = s;
    }

  auto& b = wobj.shootBullet(itm, pos.x,pos.y,pos.z, dx,dy,dz, DynamicWorld::spellSpeed);
  return b;
  }

Bullet& World::shootBullet(const Item &itm, const Npc &npc, const Npc *target, const Interactive* inter) {
  float dx  = 1.f, dy = 0.f, dz = 0.f;
  auto  pos = npc.position();
  auto  bn  = npc.mapWeaponBone();
  pos+=bn;

  if(target!=nullptr) {
    auto  tgPos = target->position();
    float y1    = target->centerY();
    float y0    = pos.y;

    dx = tgPos.x-pos.x;
    dy = y1-y0;
    dz = tgPos.z-pos.z;

    float lxz   = std::sqrt(dx*dx+0*0+dz*dz);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dy = (y1-y0)/t + 0.5f*DynamicWorld::gravity*t;
    dx/=t;
    dz/=t;
    } else
  if(inter!=nullptr) {
    auto  tgPos = inter->position();
    float y1    = tgPos.y;
    float y0    = pos.y;

    dx = tgPos.x-pos.x;
    dy = y1-y0;
    dz = tgPos.z-pos.z;

    float lxz   = std::sqrt(dx*dx+0*0+dz*dz);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dy = (y1-y0)/t + 0.5f*DynamicWorld::gravity*t;
    dx/=t;
    dz/=t;
    } else {
    float a = npc.rotationRad()-float(M_PI/2);
    float c = std::cos(a), s = std::sin(a);
    dx = c;
    dz = s;
    }

  return wobj.shootBullet(itm, pos.x,pos.y,pos.z, dx,dy,dz, DynamicWorld::bulletSpeed);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,perc);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item &item, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,item,perc);
  }

Sound World::addWeaponsSound(Npc &self, Npc &other) {
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
    "FL", //"LE", //MAT_LEATHER,
    "SA"  //MAT_CLAY,
    "ST", //MAT_GLAS,
    };
  auto p0 = self.position();
  auto p1 = other.position();

  const char* selfMt="";
  const char* othMt ="FL";

  if(self.isMonster()) // CS_AM?
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
  if(self.isMonster() || self.inventory().activeWeapon()==nullptr)
    std::snprintf(buf,sizeof(buf),"CS_MAM_%s_%s",selfMt,othMt); else
    std::snprintf(buf,sizeof(buf),"CS_IAM_%s_%s",selfMt,othMt);
  auto mid = (p0+p1)*0.5f;
  return wsound.addSound(buf, mid.x, mid.y, mid.z,2500.f,false);
  }

void World::addLandHitSound(float x,float y,float z,uint8_t m0, uint8_t m1) {
  // ItemMaterial
  static const char* mat[]={
    "WO",
    "ST",
    "ME",
    "FL", //"LE", //MAT_LEATHER,
    "SA"  //MAT_CLAY,
    "ST", //MAT_GLAS,
    };

  const char *sm0 = "ME";
  const char *sm1 = "ME";

  sm0 = mat[m0];
  sm1 = mat[m1];

  char buf[128]={};
  std::snprintf(buf,sizeof(buf),"CS_IHL_%s_%s",sm0,sm1);
  wsound.addSound(buf, x,y,z,2500.f,false).play();
  }

void World::addBlockSound(Npc &self, Npc &other) {
  // ItemMaterial
  auto p0 = self.position();
  auto p1 = other.position();

  const char* selfMt="ME";
  const char* othMt ="ME";

  if(self.isMonster())
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
  auto mid = (p0+p1)*0.5f;
  wsound.addSound(buf, mid.x, mid.y, mid.z,2500.f,false).play();
  }

bool World::isInListenerRange(const Tempest::Vec3& pos) const {
  return wsound.isInListenerRange(pos,WorldSound::talkRange);
  }

Sound World::addDlgSound(const char* s, float x, float y, float z, float range, uint64_t& timeLen) {
  return wsound.addDlgSound(s,x,y,z,range,timeLen);
  }

Sound World::addSoundEffect(const char *s, float x, float y, float z, float range, bool freeSlot) {
  return wsound.addSound(s,x,y,z,range,freeSlot);
  }

Sound World::addSoundRaw(const char *s, float x, float y, float z, float range, bool freeSlot) {
  return wsound.addSoundRaw(s,x,y,z,range,freeSlot);
  }

Sound World::addSoundRaw3d(const char* s, float x, float y, float z, float range) {
  return wsound.addSound3d(s,x,y,z,range);
  }

void World::addTrigger(AbstractTrigger* trigger) {
  wobj.addTrigger(trigger);
  }

void World::addInteractive(Interactive* inter) {
  wobj.addInteractive(inter);
  }

void World::addStartPoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, const char* name) {
  wmatrix->addStartPoint(pos,dir,name);
  }

void World::addFreePoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, const char* name) {
  wmatrix->addFreePoint(pos,dir,name);
  }

void World::addSound(const ZenLoad::zCVobData& vob) {
  if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSound ||
     vob.vobType==ZenLoad::zCVobData::VT_zCVobSoundDaytime) {
    wsound.addSound(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusic) {
    wsound.addZone(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusicDefault) {
    wsound.setDefaultZone(vob);
    }
  }

void World::invalidateVobIndex() {
  wobj.invalidateVobIndex();
  }

void World::triggerOnStart(bool firstTime) {
  wobj.triggerOnStart(firstTime);
  }

const WayPoint *World::findPoint(const char *name, bool inexact) const {
  return wmatrix->findPoint(name,inexact);
  }

const WayPoint* World::findWayPoint(const Tempest::Vec3& pos) const {
  return findWayPoint(pos.x,pos.y,pos.z);
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
  auto pos = npc.position();
  pos.y+=npc.translateY();

  return wmatrix->findFreePoint(pos.x,pos.y,pos.z,name,[&npc](const WayPoint& wp) -> bool {
    if(wp.isLocked())
      return false;
    if(!npc.canSeeNpc(wp.x,wp.y+10,wp.z,true))
      return false;
    return true;
    });
  }

const WayPoint *World::findFreePoint(const Tempest::Vec3& pos, const char* name) const {
  return wmatrix->findFreePoint(pos.x,pos.y,pos.z,name,[](const WayPoint& wp) -> bool {
    if(wp.isLocked())
      return false;
    return true;
    });
  }

const WayPoint *World::findNextFreePoint(const Npc &npc, const char *name) const {
  auto pos = npc.position();
  pos.y+=npc.translateY();
  auto cur = npc.currentWayPoint();
  auto wp  = wmatrix->findFreePoint(pos.x,pos.y,pos.z,name,[cur,&npc](const WayPoint& wp) -> bool {
    if(wp.isLocked() || &wp==cur)
      return false;
    if(!npc.canSeeNpc(wp.x,wp.y+10,wp.z,true))
      return false;
    return true;
    });
  return wp;
  }

const WayPoint *World::findNextPoint(const WayPoint &pos) const {
  return wmatrix->findNextPoint(pos.x,pos.y,pos.z);
  }

void World::detectNpcNear(std::function<void (Npc &)> f) {
  wobj.detectNpcNear(f);
  }

void World::detectNpc(const Tempest::Vec3& p, const float r, const std::function<void(Npc&)>& f) {
  wobj.detectNpc(p.x,p.y,p.z,r,f);
  }

void World::detectItem(const Tempest::Vec3& p, const float r, const std::function<void(Item&)>& f) {
  wobj.detectItem(p.x,p.y,p.z,r,f);
  }

WayPath World::wayTo(const Npc &pos, const WayPoint &end) const {
  auto p     = pos.position();
  auto point = pos.currentWayPoint();
  if(point && !point->isFreePoint() && MoveAlgo::isClose(pos.position(),*point)){
    return wmatrix->wayTo(*point,end);
    }
  return wmatrix->wayTo(p.x,p.y,p.z,end);
  }

WayPath World::wayTo(float npcX, float npcY, float npcZ, const WayPoint &end) const {
  return wmatrix->wayTo(npcX,npcY,npcZ,end);
  }

GameScript &World::script() const {
  return *game.script();
  }

const VersionInfo& World::version() const {
  return game.version();
  }

void World::assignRoomToGuild(const char* r, int32_t guildId) {
  std::string room = r;
  for(auto& i:room)
    i = char(std::toupper(i));

  if(auto rx=portalAt(room)){
    rx->guild=guildId;
    return;
    }

  Tempest::Log::d("room not found: ",room);
  }

int32_t World::guildOfRoom(const Tempest::Vec3& pos) {
  const std::string& tg = roomAt(pos);
  if(auto room=portalAt(tg)) {
    if(room->guild==GIL_PUBLIC) //FIXME: proper portal implementation
      return room->guild;
    }
  return GIL_NONE;
  }

int32_t World::guildOfRoom(const char* portalName) {
  if(portalName==nullptr)
    return -1;

  const char* b=std::strchr(portalName,':');
  if(b==nullptr)
    return -1;
  b++;

  const char* e=std::strchr(b,'_');
  size_t      size=0;
  if(e==nullptr)
    size = std::strlen(b); else
    size = size_t(std::distance(b,e));

  for(size_t i=0;i<bsp.sectors.size();++i) {
    auto& s = bsp.sectors[i].name;
    if(s.size()!=size)
      continue;

    if(std::memcmp(s.c_str(),b,size)==0)
      return bspSectors[i].guild;
    }
  return GIL_NONE;
  }
