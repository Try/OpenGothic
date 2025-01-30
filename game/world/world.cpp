#include "world.h"

#include <functional>
#include <future>
#include <cctype>

#include <Tempest/Log>
#include <Tempest/Painter>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualfx.h"
#include "world/objects/globalfx.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/triggers/abstracttrigger.h"
#include "world/triggers/cscamera.h"
#include "game/globaleffects.h"
#include "game/serialize.h"
#include "utils/string_frm.h"
#include "gothic.h"
#include "focus.h"
#include "resources.h"

const char* materialTag(ItemMaterial src) {
  switch(src) {
    case ItemMaterial::MAT_WOOD:
      return "WO";
    case ItemMaterial::MAT_STONE:
      return "ST";
    case ItemMaterial::MAT_METAL:
      return "ME";
    case ItemMaterial::MAT_LEATHER:
      return "LE";
    case ItemMaterial::MAT_CLAY:
      return "CL";
    case ItemMaterial::MAT_GLAS:
      return "GL";
    case ItemMaterial::MAT_COUNT:
      return "UD";
    }
  return "UD";
  }

const char* materialTag(zenkit::MaterialGroup src) {
  switch(src) {
    case zenkit::MaterialGroup::UNDEFINED:
    case zenkit::MaterialGroup::NONE:
      return "UD";
    case zenkit::MaterialGroup::METAL:
      return "ME";
    case zenkit::MaterialGroup::STONE:
      return "ST";
    case zenkit::MaterialGroup::WOOD:
      return "WO";
    case zenkit::MaterialGroup::EARTH:
      return "EA";
    case zenkit::MaterialGroup::WATER:
      return "WA";
    case zenkit::MaterialGroup::SNOW:
      return "SA"; // sand?
    }
  return "UD";
  }

World::World(GameSession& game, std::string_view file, bool startup, std::function<void(int)> loadProgress)
  :wname(std::move(file)), game(game), wsound(game,*this), wobj(*this) {
  const auto* entry = Resources::vdfsIndex().find(wname);

  if(entry == nullptr) {
    Tempest::Log::e("unable to open Zen-file: \"",wname,"\"");
    return;
    }

  try {
    auto          buf = entry->open_read();
    zenkit::World world;
    world.load(buf.get(), version().game == 1 ? zenkit::GameVersion::GOTHIC_1
                                              : zenkit::GameVersion::GOTHIC_2);

    loadProgress(20);
    auto& worldMesh = world.world_mesh;

    auto wdynamicFut = std::async(std::launch::async, [&]() {
      Workers::setThreadName("Loading: BVH thread");
      return std::unique_ptr<DynamicWorld>(new DynamicWorld(*this,worldMesh));
      });
    auto wviewFut = std::async(std::launch::async, [&]() {
      Workers::setThreadName("Loading: PackedMesh thread");
      PackedMesh vmesh(worldMesh,PackedMesh::PK_VisualLnd);
      return std::unique_ptr<WorldView>(new WorldView(*this,vmesh));
      });

    loadProgress(30);

    {
      bsp.nodes             = std::move(world.world_bsp_tree.nodes);
      bsp.sectors           = std::move(world.world_bsp_tree.sectors);
      bsp.leaf_node_indices = std::move(world.world_bsp_tree.leaf_node_indices);
      bsp.sectorsData.resize(bsp.sectors.size());
      world.world_bsp_tree  = zenkit::BspTree();
    }
    loadProgress(50);

    wview = wviewFut.get();
    loadProgress(60);

    wdynamic = wdynamicFut.get();
    loadProgress(70);

    globFx.reset(new GlobalEffects(*this));
    wmatrix.reset(new WayMatrix(*this,world.world_way_net));
    for(auto& vob:world.world_vobs)
      wobj.addRoot(vob,startup);

    wmatrix->buildIndex();
    loadProgress(100);
    }
  catch(...) {
    Tempest::Log::e("unable to load landscape mesh");
    throw;
    }
  }

World::~World() {
  }

void World::createPlayer(std::string_view cls) {
  size_t id = script().findSymbolIndex(cls);
  if(id==size_t(-1))
    return;
  std::string_view waypoint = wmatrix->startPoint().name;
  auto             npc      = std::make_unique<Npc>(*this, id, waypoint, Npc::ProcessPolicy::Player);
  npcPlayer = wobj.insertPlayer(std::move(npc), waypoint);
  game.script()->setInstanceNPC("HERO",*npcPlayer);
  }

void World::insertPlayer(std::unique_ptr<Npc> &&npc, std::string_view waypoint) {
  if(npc==nullptr)
    return;
  npcPlayer = wobj.insertPlayer(std::move(npc),waypoint);
  game.script()->setInstanceNPC("HERO",*npcPlayer);
  }

void World::setPlayer(Npc* npc) {
  if(npc==nullptr)
    return;
  npcPlayer->setProcessPolicy(Npc::ProcessPolicy::AiNormal);
  if(!npcPlayer->isDead()) {
    npcPlayer->resumeAiRoutine();
    }
  
  npc->setProcessPolicy(Npc::ProcessPolicy::Player);
  npc->clearState(true);
  npc->clearAiQueue();

  for(size_t i=0;i<PERC_Count;++i)
    npc->setPerceptionDisable(PercType(i));
  
  npcPlayer = npc;
  }

void World::postInit() {
  // NOTE: level inspector override player stats globaly
  // lvlInspector.reset(new Npc(*this,script().getSymbolIndex("PC_Levelinspektor"),""));

  // game.script()->inserNpc("Snapper",wmatrix->startPoint().name);
  }

void World::load(Serialize &fin) {
  fin.setContext(this);
  fin.setEntry("worlds/",wname,"/world");

  uint32_t sz=0;
  fin.read(sz);
  std::string tag;
  for(size_t i=0;i<sz;++i) {
    int32_t guild=GIL_NONE;
    fin.read(tag,guild);
    if(auto p = portalAt(tag))
      p->guild = guild;
    }

  wobj.load(fin);
  npcPlayer = wobj.findHero();
  }

void World::save(Serialize &fout) {
  fout.setContext(this);
  fout.setEntry("worlds/",wname,"/world");

  fout.write(uint32_t(bsp.sectorsData.size()));
  for(size_t i=0;i<bsp.sectorsData.size();++i) {
    fout.write(bsp.sectors[i].name,bsp.sectorsData[i].guild);
    }

  wobj.save(fout);
  }

uint32_t World::npcId(const Npc *ptr) const {
  return wobj.npcId(ptr);
  }

Npc *World::npcById(uint32_t id) {
  if(id<wobj.npcCount())
    return &wobj.npc(id);
  return nullptr;
  }

uint32_t World::npcCount() const {
  return uint32_t(wobj.npcCount());
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
    vfx = vfx->emFXCreate;
    }
  wobj.stopEffect(root);
  }

GlobalFx World::addGlobalEffect(std::string_view what, uint64_t len, const std::string* argv, size_t argc) {
  return globFx->startEffect(what,len,argv,argc);
  }

MeshObjects::Mesh World::addView(std::string_view visual) const {
  return addView(visual,0,0,0);
  }

MeshObjects::Mesh World::addView(std::string_view visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return view()->addView(visual,headTex,teetTex,bodyColor);
  }

MeshObjects::Mesh World::addView(const zenkit::IItem& itm) {
  return view()->addView(itm.visual,itm.material,0,itm.material);
  }

MeshObjects::Mesh World::addView(const ProtoMesh* visual) {
  return view()->addView(visual);
  }

MeshObjects::Mesh World::addAtachView(const ProtoMesh::Attach& visual, const int32_t version) {
  return view()->addAtachView(visual,version);
  }

MeshObjects::Mesh World::addStaticView(const ProtoMesh* visual, bool staticDraw) {
  return view()->addStaticView(visual,staticDraw);
  }

MeshObjects::Mesh World::addStaticView(std::string_view visual) {
  return view()->addStaticView(visual);
  }

MeshObjects::Mesh World::addDecalView(const zenkit::VisualDecal& decal) {
  return view()->addDecalView(decal);
  }

LightGroup::Light World::addLight(const zenkit::VLight& vob) {
  return view()->addLight(vob);
  }

LightGroup::Light World::addLight(std::string_view preset) {
  return view()->addLight(preset);
  }

void World::updateAnimation(uint64_t dt) {
  wobj.updateAnimation(dt);
  }

void World::resetPositionToTA() {
  wobj.resetPositionToTA();
  }

std::unique_ptr<Npc> World::takeHero() {
  return wobj.takeNpc(npcPlayer);
  }

Item* World::findItemByInstance(size_t instance, size_t n) {
  return wobj.findItemByInstance(instance,n);
  }

Npc *World::findNpcByInstance(size_t instance, size_t n) {
  return wobj.findNpcByInstance(instance,n);
  }

std::string_view World::roomAt(const Tempest::Vec3& p) {
  if(bsp.nodes.empty())
    return "";

  const auto* node=&bsp.nodes[0];

  while(true) {
    const auto v    = node->plane;
    float        sgn  = v.x*p.x + v.y*p.y + v.z*p.z - v.w;
    uint32_t     next = (sgn>0) ? uint32_t(node->front_index) : uint32_t(node->back_index);
    if(next>=bsp.nodes.size())
      break;

    node = &bsp.nodes[next];
    }

  if(node->bbox.min.x <= p.x && p.x <node->bbox.max.x &&
     node->bbox.min.y <= p.y && p.y <node->bbox.max.y &&
     node->bbox.min.z <= p.z && p.z <node->bbox.max.z) {
    return roomAt(*node);
    }

  return "";
  }

std::string_view World::roomAt(const zenkit::BspNode& node) {
  const std::string* ret=nullptr;
  size_t       count=0;
  auto         id = &node-bsp.nodes.data();(void)id;

  for(auto& i:bsp.sectors) {
    for(auto r:i.node_indices)
      if(r<bsp.leaf_node_indices.size()){
        size_t idx = bsp.leaf_node_indices[r];
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

World::BspSector* World::portalAt(std::string_view tag) {
  if(tag.empty())
    return nullptr;

  for(size_t i=0;i<bsp.sectors.size();++i)
    if(bsp.sectors[i].name==tag)
      return &bsp.sectorsData[i];
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

Focus World::validateFocus(const Focus &def) {
  Focus ret = def;
  ret.npc         = wobj.validateNpc(ret.npc);
  ret.interactive = wobj.validateInteractive(ret.interactive);
  ret.item        = wobj.validateItem(ret.item);

  return ret;
  }

Focus World::findFocus(const Npc &pl, const Focus& def) {
  auto  opt      = WorldObjects::NoFlg;
  auto  collAlgo = TARGET_COLLECT_FOCUS;
  auto  collType = TARGET_TYPE_ALL;
  auto& policy   = searchPolicy(pl,collAlgo,collType,opt);

  WorldObjects::SearchOpt optNpc {policy.npc_range1,  policy.npc_range2,  policy.npc_azi,  collAlgo, collType, opt};
  WorldObjects::SearchOpt optMob {policy.mob_range1,  policy.mob_range2,  policy.mob_azi,  collAlgo};
  WorldObjects::SearchOpt optItm {policy.item_range1, policy.item_range2, policy.item_azi, collAlgo, collType};

  auto n     = policy.npc_prio <0 ? nullptr : wobj.findNpcNear    (pl,def.npc,        optNpc);
  auto it    = policy.item_prio<0 ? nullptr : wobj.findItem       (pl,def.item,       optItm);
  auto inter = policy.mob_prio <0 ? nullptr : wobj.findInteractive(pl,def.interactive,optMob);
  auto ws = pl.weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
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
  if(npcPlayer==nullptr || npcPlayer->interactive()!=nullptr)
    return Focus();
  return findFocus(*npcPlayer,def);
  }

bool World::testFocusNpc(Npc* def) {
  auto  opt      = WorldObjects::NoFlg;
  auto  collAlgo = TARGET_COLLECT_FOCUS;
  auto  collType = TARGET_TYPE_ALL;
  auto& policy   = searchPolicy(*npcPlayer,collAlgo,collType,opt);

  WorldObjects::SearchOpt optNpc{policy.npc_range1, policy.npc_range2, policy.npc_azi, collAlgo, collType, opt};
  return wobj.testFocusNpc(*npcPlayer,def,optNpc);
  }

Interactive *World::availableMob(const Npc &pl, std::string_view name) {
  return wobj.availableMob(pl,name);
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
  if(e.timeBarrier > this->tickCount()) {
    triggerEvent(std::move(e));
    return;
    }

  bool emitted = wobj.execTriggerEvent(e);
  if(!emitted) {
    emitted = wsound.execTriggerEvent(e);
    }

  if(!emitted) {
    if(e.target=="EVT_LEFT_ROOM_01_TRAP_MOVER_FOR_DMG_MASTER" ||
       e.target=="EVT_LEFT_UP_01_TOGGLE_TRIGGER_01" ||
       e.target=="EVT_RIGHT_ROOM_01_SPAWN_ROT_02_SOUND" ||
       e.target=="NULL")
      return; // known problem on dragonisland.zen, skip for now
    Tempest::Log::d("unable to process trigger: \"",e.target,"\"");
    }
  }

void World::enableDefTrigger(AbstractTrigger& t) {
  wobj.enableDefTrigger(t);
  }

void World::enableTicks(AbstractTrigger& t) {
  wobj.enableTicks(t);
  }

void World::disableTicks(AbstractTrigger& t) {
  wobj.disableTicks(t);
  }

void World::setCurrentCs(CsCamera* cs) {
  wobj.setCurrentCs(cs);
  }

CsCamera* World::currentCs() const {
  return wobj.currentCs();
  }

bool World::isCutsceneLock() const {
  if(auto cs = wobj.currentCs())
    return !cs->isPlayerMovable();
  return false;
  }

void World::enableCollizionZone(CollisionZone& z) {
  wobj.enableCollizionZone(z);
  }

void World::disableCollizionZone(CollisionZone& z) {
  wobj.disableCollizionZone(z);
  }

void World::triggerChangeWorld(std::string_view world, std::string_view wayPoint) {
  game.changeWorld(world,wayPoint);
  }

void World::setMobRoutine(gtime time, std::string_view scheme, int32_t state) {
  wobj.setMobRoutine(time,scheme,state);
  }

void World::marchInteractives(DbgPainter &p) const {
  wobj.marchInteractives(p);
  }

void World::marchPoints(DbgPainter& p) const {
  wmatrix->marchPoints(p);
  }

AiOuputPipe *World::openDlgOuput(Npc &player, Npc &npc) {
  return game.openDlgOuput(player,npc);
  }

void World::aiOutputSound(Npc &player, std::string_view msg) {
  wsound.aiOutput(player.position(),msg);
  }

bool World::aiIsDlgFinished() {
  return game.aiIsDlgFinished();
  }

bool World::isTargeted(Npc& npc) {
  return wobj.isTargeted(npc);
  }

Npc *World::addNpc(std::string_view name, std::string_view at) {
  size_t id = script().findSymbolIndex(name);
  if(id==size_t(-1))
    return nullptr;
  return wobj.addNpc(id,at);
  }

Npc *World::addNpc(size_t npcInstance, std::string_view at) {
  return wobj.addNpc(npcInstance,at);
  }

Npc* World::addNpc(size_t npcInstance, const Tempest::Vec3& at) {
  return wobj.addNpc(npcInstance,at);
  }

void World::removeNpc(Npc& npc) {
  return wobj.removeNpc(npc);
  }

Item *World::addItem(size_t itemInstance, std::string_view at) {
  return wobj.addItem(itemInstance,at);
  }

Item* World::addItem(const zenkit::VItem& vob) {
  return wobj.addItem(vob);
  }

Item* World::addItem(size_t itemInstance, const Tempest::Vec3& pos) {
  return wobj.addItem(itemInstance, pos);
  }

Item* World::addItemDyn(size_t itemInstance, const Tempest::Matrix4x4& pos, size_t owner) {
  return wobj.addItemDyn(itemInstance, pos, owner);
  }

std::unique_ptr<Item> World::takeItem(Item &it) {
  return wobj.takeItem(it);
  }

void World::removeItem(Item& it) {
  wobj.removeItem(it);
  }

size_t World::hasItems(std::string_view tag, size_t itemCls) {
  return wobj.hasItems(tag,itemCls);
  }

Bullet& World::shootSpell(const Item &itm, const Npc &npc, const Npc *target) {
  Tempest::Vec3   dir     = {1.f,0.f,0.f};
  auto            pos     = npc.position();
  const VisualFx* vfx     = script().spellVfx(itm.spellId());
  float           tgRange = vfx==nullptr ? 0 : vfx->emTrjTargetRange;

  if(target!=nullptr) {
    auto  tgPos = target->position();
    if(vfx!=nullptr) {
      pos   = npc.mapBone(vfx->emTrjOriginNode);
      tgPos = target->mapBone(vfx->emTrjTargetNode);
      }
    dir = tgPos-pos;
    } else {
    float a = npc.rotationRad()-float(M_PI/2);
    dir.x = std::cos(a);
    dir.z = std::sin(a);
    pos  = npc.mapWeaponBone();
    }

  auto& b = wobj.shootBullet(itm, pos, dir, tgRange, DynamicWorld::spellSpeed);
  b.setTarget(target);
  return b;
  }

Bullet& World::shootBullet(const Item &itm, const Npc &npc, const Npc *target, const Interactive* inter) {
  Tempest::Vec3 dir = {1.f,0.f,0.f};
  auto          pos = npc.mapWeaponBone();

  if(target!=nullptr) {
    dir = target->centerPosition() - pos;

    float lxz   = std::sqrt(dir.x*dir.x+0*0+dir.z*dir.z);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dir/=t;
    dir.y += 0.5f*DynamicWorld::gravity*t;
    } else
  if(inter!=nullptr) {
    auto  tgPos = inter->position();
    dir = tgPos-pos;

    float lxz   = std::sqrt(dir.x*dir.x+0*0+dir.z*dir.z);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dir/=t;
    dir.y += 0.5f*DynamicWorld::gravity*t;
    } else {
    float a = npc.rotationRad()-float(M_PI/2);
    dir.x = std::cos(a);
    dir.z = std::sin(a);
    }

  auto& b = wobj.shootBullet(itm, pos, dir, 0, DynamicWorld::bulletSpeed);
  b.setTarget(target);
  return b;
  }

void World::sendPassivePerc(Npc &self, Npc &other, Item& item, int32_t perc) {
  wobj.sendPassivePerc(self,other,nullptr,&item,perc);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  wobj.sendPassivePerc(self,other,&victum,nullptr,perc);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item& item, int32_t perc) {
  wobj.sendPassivePerc(self,other,&victum,&item,perc);
  }

void World::sendImmediatePerc(Npc& self, Npc& other, Npc& victum, int32_t perc) {
  wobj.sendImmediatePerc(self,other,victum,nullptr,perc);
  }

void World::sendImmediatePerc(Npc& self, Npc& other, Npc& victum, Item& item, int32_t perc) {
  wobj.sendImmediatePerc(self,other,victum,&item,perc);
  }

Sound World::addWeaponHitEffect(Npc& src, const Bullet* srcArrow, Npc& reciver) {
  auto p0 = src.position();
  auto p1 = reciver.position();

  Tempest::Matrix4x4 pos;
  pos.identity();
  pos.translate((p0+p1)*0.5f);

  const char* armor = "FL";
  if(auto a = reciver.currentArmour()) {
    auto m = ItemMaterial(a->handle().material);
    // NOTE: in vanilla only those sfx are defined for armor
    if(m==ItemMaterial::MAT_METAL || m==ItemMaterial::MAT_WOOD)
      armor = materialTag(m);
    }

  if(srcArrow!=nullptr && !srcArrow->isSpell()) {
    auto m = ItemMaterial(srcArrow->itemMaterial());
    return addHitEffect(materialTag(m),armor,"IAM",pos);
    }

  if(auto w = src.inventory().activeWeapon()) {
    auto m = ItemMaterial(w->handle().material);
    return addHitEffect(materialTag(m),armor,"IAM",pos);
    }

  if(src.isMonster())
    return addHitEffect("JA",armor,"MAM",pos); else
    return addHitEffect("FI",armor,"MAM",pos);
  }

Sound World::addLandHitEffect(ItemMaterial src, zenkit::MaterialGroup reciver, const Tempest::Matrix4x4& pos) {
  // IHI - item hits item
  // IHL - Item hits Level
  return addHitEffect(materialTag(src),materialTag(reciver),"IHL",pos);
  }

Sound World::addWeaponBlkEffect(ItemMaterial src, ItemMaterial reciver, const Tempest::Matrix4x4& pos) {
  // IAI - item attacks item
  return addHitEffect(materialTag(src),materialTag(reciver),"IAI",pos);
  }

Sound World::addHitEffect(std::string_view src, std::string_view dst, std::string_view scheme, const Tempest::Matrix4x4& pos) {
  Tempest::Vec3 pos3;
  pos.project(pos3);

  string_frm sound("CS_",scheme,'_',src,'_',dst);
  auto ret = Sound(*this,::Sound::T_Regular,sound,pos3,2500.f,false);

  string_frm buf("CPFX_",scheme,'_',src,'_',dst);
  if(Gothic::inst().loadParticleFx(buf,true)==nullptr) {
    if(dst=="ME")
      buf = string_frm("CPFX_",scheme,"_METAL");
    else if(dst=="WO")
      buf = string_frm("CPFX_",scheme,"_WOOD");
    else if(dst=="ST")
      buf = string_frm("CPFX_",scheme,"_STONE");
    else
      return ret;
    }
  if(Gothic::inst().loadParticleFx(buf,true)==nullptr)
    return ret;

  Effect e(PfxEmitter(*this,buf),"");
  e.setObjMatrix(pos);
  e.setActive(true);
  runEffect(std::move(e));
  return ret;
  }

bool World::isInSfxRange(const Tempest::Vec3& pos) const {
  return wsound.isInListenerRange(pos,WorldSound::talkRange);
  }

bool World::isInPfxRange(const Tempest::Vec3& p) const {
  return wview->isInPfxRange(p);
  }

void World::addDlgSound(std::string_view s, const Tempest::Vec3& pos, float range, uint64_t& timeLen) {
  auto sfx = wsound.addDlgSound(s,pos,range,timeLen);
  sfx.play();
  }

void World::addTrigger(AbstractTrigger* trigger) {
  wobj.addTrigger(trigger);
  }

void World::addInteractive(Interactive* inter) {
  wobj.addInteractive(inter);
  }

void World::addStartPoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name) {
  wmatrix->addStartPoint(pos,dir,name);
  }

void World::addFreePoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name) {
  wmatrix->addFreePoint(pos,dir,name);
  }

void World::addSound(const zenkit::VirtualObject& vob) {
  if(vob.type==zenkit::VirtualObjectType::zCVobSound || vob.type==zenkit::VirtualObjectType::zCVobSoundDaytime) {
    wsound.addSound(reinterpret_cast<const zenkit::VSound&>(vob));
    }
  else if(vob.type==zenkit::VirtualObjectType::oCZoneMusic) {
    wsound.addZone(reinterpret_cast<const zenkit::VZoneMusic&>(vob));
    }
  else if(vob.type==zenkit::VirtualObjectType::oCZoneMusicDefault) {
    wsound.setDefaultZone(reinterpret_cast<const zenkit::VZoneMusicDefault&>(vob));
    }
  }

void World::invalidateVobIndex() {
  wobj.invalidateVobIndex();
  }

const zenkit::IFocus& World::searchPolicy(const Npc& pl, TargetCollect& collAlgo, TargetType& collType, WorldObjects::SearchFlg& opt) const {
  opt      = WorldObjects::NoFlg;
  collAlgo = TARGET_COLLECT_FOCUS;
  collType = TARGET_TYPE_ALL;

  switch(pl.weaponState()) {
    case WeaponState::Fist:
    case WeaponState::W1H:
    case WeaponState::W2H:
      opt = WorldObjects::NoDeath;
      return game.script()->focusMelee();
    case WeaponState::Bow:
    case WeaponState::CBow:
      opt = WorldObjects::SearchFlg(WorldObjects::NoDeath | WorldObjects::NoUnconscious);
      return game.script()->focusRange();
    case WeaponState::Mage:{
      if(auto weapon = pl.inventory().activeWeapon()) {
        int32_t id  = weapon->spellId();
        auto&   spl = script().spellDesc(id);
        collAlgo = TargetCollect(spl.target_collect_algo);
        collType = TargetType(spl.target_collect_type);
        }
      opt = WorldObjects::SearchFlg(WorldObjects::NoDeath | WorldObjects::NoUnconscious);
      return game.script()->focusMage();
      }
    case WeaponState::NoWeapon:
      return game.script()->focusNorm();
    }
  return game.script()->focusNorm();
  }

void World::triggerOnStart(bool firstTime) {
  wobj.triggerOnStart(firstTime);
  }

const WayPoint *World::findPoint(std::string_view name, bool inexact) const {
  return wmatrix->findPoint(name,inexact);
  }

const WayPoint* World::findWayPoint(const Tempest::Vec3& pos) const {
  return wmatrix->findWayPoint(pos,[](const WayPoint&){ return true; });
  }

const WayPoint* World::findWayPoint(const Tempest::Vec3& pos, const std::function<bool(const WayPoint&)>& f) const {
  return wmatrix->findWayPoint(pos,f);
  }

const WayPoint *World::findFreePoint(const Npc &npc, std::string_view name) const {
  if(auto p = npc.currentWayPoint()){
    if(p->isFreePoint() && p->checkName(name)) {
      return p;
      }
    }
  auto pos = npc.position();
  pos.y+=npc.translateY();

  return wmatrix->findFreePoint(pos,name,[&npc](const WayPoint& wp) -> bool {
    if(wp.isLocked())
      return false;
    if(!npc.canRayHitPoint(Tempest::Vec3(wp.x,wp.y+10,wp.z),true))
      return false;
    return true;
    });
  }

const WayPoint *World::findFreePoint(const Tempest::Vec3& pos, std::string_view name) const {
  return wmatrix->findFreePoint(pos,name,[](const WayPoint& wp) -> bool {
    if(wp.isLocked())
      return false;
    return true;
    });
  }

const WayPoint *World::findNextFreePoint(const Npc &npc, std::string_view name) const {
  auto pos = npc.position();
  pos.y+=npc.translateY();
  auto cur = npc.currentWayPoint();
  if(cur!=nullptr && !cur->checkName(name)) {
    cur = nullptr;
    }
  auto filter = [&](const WayPoint& wp) {
    if(wp.isLocked() || &wp==cur)
      return false;
    if(!npc.canRayHitPoint(Tempest::Vec3(wp.x,wp.y+10,wp.z),true))
      return false;
    return true;
    };
  auto wp  = wmatrix->findFreePoint(pos, name, filter);
  return wp;
  }

const WayPoint* World::findNextWayPoint(const Npc &npc) const {
  auto pos = npc.position();
  pos.y+=npc.translateY();

  auto nearest = npc.currentWayPoint();
  if(nearest==nullptr || nearest->isFreePoint()) {
    nearest = findWayPoint(pos);
    }
  auto filter  = [&](const WayPoint& wp) {
    if(!npc.canRayHitPoint(Tempest::Vec3(wp.x,wp.y+10,wp.z),true))
      return false;
    return nearest != &wp;
    };
  return nearest ? findWayPoint(pos, filter) : nullptr;
  }

const WayPoint *World::findNextPoint(const WayPoint &pos) const {
  return wmatrix->findNextPoint(pos.position());
  }

const WayPoint& World::startPoint() const {
  return wmatrix->startPoint();
  }

const WayPoint& World::deadPoint() const {
  return wmatrix->deadPoint();
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

WayPath World::wayTo(const Npc &npc, const WayPoint &end) const {
  auto p     = npc.position();

  auto begin = npc.currentWayPoint();
  if(begin==&end && MoveAlgo::isClose(npc.position(),end)) {
    return WayPath();
    }
  if(begin && !begin->isFreePoint() && MoveAlgo::isClose(npc.position(),*begin)) {
    return wmatrix->wayTo(&begin,1,p,end);
    }
  auto near = wmatrix->findWayPoint(p, [&npc](const WayPoint &wp) {
    if(!npc.canRayHitPoint(Tempest::Vec3(wp.x,wp.y+10,wp.z),true))
      return false;
    return true;
    });
  if(near==nullptr)
    return WayPath();

  if(MoveAlgo::isClose(p,*near) && near==&end)
    return WayPath();

  std::vector<const WayPoint*> wpoint;
  wpoint.push_back(near);
  for(auto& i:near->connections()) {
    auto p = i.point->position();
    if(npc.canRayHitPoint(Tempest::Vec3(p.x,p.y+10,p.z)))
      wpoint.push_back(i.point);
    }

  return wmatrix->wayTo(wpoint.data(),wpoint.size(),p,end);
  }

GameScript &World::script() const {
  return *game.script();
  }

const VersionInfo& World::version() const {
  return game.version();
  }

void World::assignRoomToGuild(std::string_view r, int32_t guildId) {
  auto room = std::string(r);
  for(auto& i:room)
    i = char(std::toupper(i));

  if(auto rx=portalAt(room)){
    rx->guild=guildId;
    return;
    }

  Tempest::Log::d("room not found: ",room);
  }

int32_t World::guildOfRoom(const Tempest::Vec3& pos) {
  std::string_view tg = roomAt(pos);
  if(auto room=portalAt(tg)) {
    if(room->guild==GIL_PUBLIC) //FIXME: proper portal implementation
      return room->guild;
    }
  return GIL_NONE;
  }

int32_t World::guildOfRoom(std::string_view portalName) {
  size_t b = portalName.find(':');
  if(b==std::string::npos)
    return -1;
  b++;

  size_t e=portalName.find('_');
  if(e==std::string::npos || b>e)
    return -1;

  auto name = portalName.substr(b,e-b);

  for(size_t i=0;i<bsp.sectors.size();++i) {
    auto& s = bsp.sectors[i].name;
    if(s==name)
      return bsp.sectorsData[i].guild;
    }
  return GIL_NONE;
  }
