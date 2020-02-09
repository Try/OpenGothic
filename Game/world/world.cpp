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
#include "graphics/submesh/packedmesh.h"
#include "graphics/visualfx.h"
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

  // ZenLoad::PackedMesh mesh;
  //worldMesh->packMesh(mesh, 1.f, false);
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  PackedMesh vmesh(*worldMesh,PackedMesh::PK_Visual);
  PackedMesh pmesh(*worldMesh,PackedMesh::PK_Physic);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,pmesh));
  wview.reset   (new WorldView(*this,vmesh,storage));
  loadProgress(70);

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob,true);
    }
  wmatrix->buildIndex();
  bsp = std::move(world.bspTree);
  bspSectors.resize(bsp.sectors.size());

  wobj.triggerOnStart(true);
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

  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  PackedMesh vmesh(*worldMesh,PackedMesh::PK_Visual);
  PackedMesh pmesh(*worldMesh,PackedMesh::PK_Physic);

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,pmesh));
  wview.reset   (new WorldView(*this,vmesh,storage));
  loadProgress(70);

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      loadVob(vob,false);
    }
  wmatrix->buildIndex();
  bsp = std::move(world.bspTree);
  bspSectors.resize(bsp.sectors.size());

  wobj.triggerOnStart(false);
  loadProgress(100);
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

MeshObjects::Mesh World::getView(const char* visual) const {
  return getView(visual,0,0,0);
  }

MeshObjects::Mesh World::getView(const Daedalus::ZString& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return getView(visual.c_str(),headTex,teetTex,bodyColor);
  }

MeshObjects::Mesh World::getView(const char* visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return view()->getView(visual,headTex,teetTex,bodyColor);
  }

PfxObjects::Emitter World::getView(const ParticleFx *decl) const {
  return view()->getView(decl);
  }

MeshObjects::Mesh World::getItmView(const Daedalus::ZString& visual, int32_t tex) const {
  return getItmView(visual.c_str(),tex);
  }

MeshObjects::Mesh World::getItmView(const char* visual, int32_t tex) const {
  return view()->getItmView(visual,tex);
  }

MeshObjects::Mesh World::getStaticView(const char* visual) const {
  return view()->getStaticView(visual);
  }

DynamicWorld::Item World::getPhysic(const char* visual) {
  if(auto sk=Resources::loadSkeleton(visual))
    return physic()->ghostObj(sk->bboxCol[0],sk->bboxCol[1]);
  ZMath::float3 zero={};
  return physic()->ghostObj(zero,zero);
  }

const VisualFx *World::loadVisualFx(const char *name) {
  return game.loadVisualFx(name);
  }

const ParticleFx* World::loadParticleFx(const char *name) {
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

const std::string& World::roomAt(const std::array<float,3> &arr) {
  static std::string empty;

  if(bsp.nodes.empty())
    return empty;

  const float x=arr[0], y=arr[1], z=arr[2];

  const ZenLoad::zCBspNode* node=&bsp.nodes[0];

  while(true) {
    const float* v    = node->plane.v;
    float        sgn  = v[0]*x + v[1]*y + v[2]*z - v[3];
    uint32_t     next = (sgn>0) ? node->front : node->back;
    if(next>=bsp.nodes.size())
      break;

    node = &bsp.nodes[next];
    }

  if(node->bbox3dMin.x <= x && x <node->bbox3dMax.x &&
     node->bbox3dMin.y <= y && y <node->bbox3dMax.y &&
     node->bbox3dMin.z <= z && z <node->bbox3dMax.z) {
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

void World::tick(uint64_t dt) {
  static bool doTicks=true;
  if(!doTicks)
    return;
  wobj.tick(dt);
  wdynamic->tick(dt);
  wview->tick(dt);
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

void World::triggerEvent(const TriggerEvent &e) {
  wobj.triggerEvent(e);
  }

void World::enableTicks(AbstractTrigger& t) {
  wobj.enableTicks(t);
  }

void World::disableTicks(AbstractTrigger& t) {
  wobj.disableTicks(t);
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

void World::printScreen(const char *msg, int x, int y, int time, const GthFont &font) {
  game.printScreen(msg,x,y,time,font);
  }

void World::print(const char *msg) {
  game.print(msg);
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

Item *World::addItem(size_t itemInstance, const char *at) {
  return wobj.addItem(itemInstance,at);
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
  float dx = 1.f, dy = 0.f, dz = 0.f;
  auto  pos = npc.position();
  pos[1] = npc.centerY();

  if(target!=nullptr) {
    auto  tgPos = target->position();
    float y1    = target->centerY();
    float y0    = pos[1];

    dx = tgPos[0]-pos[0];
    dy = y1-y0;
    dz = tgPos[2]-pos[2];
    } else {
    float a = npc.rotationRad()-float(M_PI/2);
    float c = std::cos(a), s = std::sin(a);
    dx = c;
    dz = s;
    }

  const int32_t     id  = itm.spellId();
  const VisualFx*   vfx = script().getSpellVFx(id);
  if(vfx!=nullptr)
    vfx->emitSound(*this,pos,SpellFxKey::Cast);

  auto& b = wobj.shootBullet(itm, pos[0],pos[1],pos[2], dx,dy,dz, DynamicWorld::spellSpeed);
  return b;
  }

Bullet& World::shootBullet(const Item &itm, const Npc &npc, const Npc *target) {
  float dx = 1.f, dy = 0.f, dz = 0.f;
  auto  pos = npc.position();
  pos[1] = npc.centerY();

  if(target!=nullptr) {
    auto  tgPos = target->position();
    float y1    = target->centerY();
    float y0    = pos[1];

    dx = tgPos[0]-pos[0];
    dy = y1-y0;
    dz = tgPos[2]-pos[2];

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

  return wobj.shootBullet(itm, pos[0],pos[1],pos[2], dx,dy,dz, DynamicWorld::bulletSpeed);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,perc);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item &item, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,item,perc);
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
  wsound.emitSound(buf, 0.5f*(p0[0]+p1[0]), 0.5f*(p0[1]+p1[1]), 0.5f*(p0[2]+p1[2]),2500.f,false);
  }

void World::emitLandHitSound(float x,float y,float z,uint8_t m0, uint8_t m1) {
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
  wsound.emitSound(buf, x,y,z,2500.f,false);
  }

void World::emitBlockSound(Npc &self, Npc &other) {
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
  wsound.emitSound(buf, 0.5f*(p0[0]+p1[0]), 0.5f*(p0[1]+p1[1]), 0.5f*(p0[2]+p1[2]),2500.f,false);
  }

bool World::isInListenerRange(const std::array<float,3> &pos) const {
  return wsound.isInListenerRange(pos);
  }

void World::emitDlgSound(const char* s, float x, float y, float z, float range, uint64_t& timeLen) {
  wsound.emitDlgSound(s,x,y,z,range,timeLen);
  }

void World::emitSoundEffect(const char *s, float x, float y, float z, float range, bool freeSlot) {
  wsound.emitSound(s,x,y,z,range,freeSlot);
  }

void World::emitSoundRaw(const char *s, float x, float y, float z, float range, bool freeSlot) {
  wsound.emitSoundRaw(s,x,y,z,range,freeSlot);
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

void World::detectNpcNear(std::function<void (Npc &)> f) {
  wobj.detectNpcNear(f);
  }

void World::detectNpc(const std::array<float,3> p, const float r, std::function<void (Npc &)> f) {
  wobj.detectNpc(p[0],p[1],p[2],r,f);
  }

void World::detectNpc(const float x, const float y, const float z, const float r, std::function<void(Npc&)> f) {
  wobj.detectNpc(x,y,z,r,f);
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

  Log::d("room not found: ",room);
  }

int32_t World::guildOfRoom(const std::array<float,3> &pos) {
  const std::string& tg = roomAt(pos);
  if(auto room=portalAt(tg)) {
    if(room->guild==GIL_PUBLIC) //FIXME: proper portal implementation
      return room->guild;
    }
  return GIL_NONE;
  }

MeshObjects::Mesh World::getView(const Daedalus::ZString& visual) const {
  return getView(visual.c_str());
  }

void World::loadVob(ZenLoad::zCVobData &vob,bool startup) {
  for(auto& i:vob.childVobs)
    loadVob(i,startup);
  vob.childVobs.clear(); // because of move

  if(vob.vobType==ZenLoad::zCVobData::VT_zCVobLevelCompo)
    return;

  if(vob.vobType==ZenLoad::zCVobData::VT_zCVob) {
    wobj.addStatic(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMobFire){
    wobj.addStatic(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMOB) {
    // Irdotar bow-triggers
    // focusOverride=true

    // Graves/Pointers
    // see focusNam
    if(startup)
      wobj.addInteractive(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMobBed ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobDoor ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobInter ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobContainer ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobSwitch){
    if(startup)
      wobj.addInteractive(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCMover){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCCodeMaster ||
          vob.vobType==ZenLoad::zCVobData::VT_zCTrigger ||
          vob.vobType==ZenLoad::zCVobData::VT_zCTriggerList ||
          vob.vobType==ZenLoad::zCVobData::VT_zCTriggerScript ||
          vob.vobType==ZenLoad::zCVobData::VT_oCTriggerWorldStart ||
          vob.vobType==ZenLoad::zCVobData::VT_oCTriggerChangeLevel){
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCMessageFilter) {
    wobj.addTrigger(std::move(vob));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobStartpoint) {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    wmatrix->addStartPoint(vob.position.x,vob.position.y,vob.position.z,dx,dy,dz,vob.vobName.c_str());
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSpot) {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    wmatrix->addFreePoint(vob.position.x,vob.position.y,vob.position.z,dx,dy,dz,vob.vobName.c_str());
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCItem) {
    if(startup)
      wobj.addItem(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSound ||
          vob.vobType==ZenLoad::zCVobData::VT_zCVobSoundDaytime) {
    wsound.addSound(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusic) {
    wsound.addZone(vob);
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusicDefault) {
    wsound.setDefaultZone(vob);
    }
  else if(vob.objectClass=="zCVobAnimate:zCVob" || // ork flags
          vob.objectClass=="zCPFXControler:zCVob"){
    wobj.addStatic(vob); //TODO: morph animation
    }
  else if(vob.objectClass=="oCTouchDamage:zCTouchDamage:zCVob" ||
          vob.objectClass=="oCMobLadder:oCMobInter:oCMOB:zCVob"){
    // NOT IMPLEMENTED
    }
  else if(vob.objectClass=="zCVobLight:zCVob" ||
          vob.objectClass=="zCVobLensFlare:zCVob" ||
          vob.objectClass=="zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob") {
    // WONT-IMPLEMENT
    }
  else {
    static std::unordered_set<std::string> cls;
    if(cls.find(vob.objectClass)==cls.end()){
      cls.insert(vob.objectClass);
      Tempest::Log::d("unknown vob class ",vob.objectClass);
      }
    }
  }
