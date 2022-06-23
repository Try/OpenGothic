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

const char* materialTag(ZenLoad::MaterialGroup src) {
  switch(src) {
    case ZenLoad::MaterialGroup::UNDEF:
    case ZenLoad::MaterialGroup::NUM_MAT_GROUPS:
      return "UD";
    case ZenLoad::MaterialGroup::METAL:
      return "ME";
    case ZenLoad::MaterialGroup::STONE:
      return "ST";
    case ZenLoad::MaterialGroup::WOOD:
      return "WO";
    case ZenLoad::MaterialGroup::EARTH:
      return "EA";
    case ZenLoad::MaterialGroup::WATER:
      return "WA";
    case ZenLoad::MaterialGroup::SNOW:
      return "SA"; // sand?
    }
  return "UD";
  }

World::World(GameSession& game, std::string file, bool startup, std::function<void(int)> loadProgress)
  :wname(std::move(file)),game(game),wsound(game,*this),wobj(*this) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(wname,Resources::vdfsIndex());
  loadProgress(1);
  if(parser.getFileSize()==0)
    Tempest::Log::e("unable to open Zen-file: \"",wname,"\"");
  parser.readHeader();

  loadProgress(10);
  ZenLoad::oCWorldData world;

  auto fver = ZenLoad::ZenParser::FileVersion::Gothic1;
  if(Gothic::inst().version().game==2)
    fver = ZenLoad::ZenParser::FileVersion::Gothic2;

  try {
    parser.readWorld(world,fver);
    }
  catch(...) {
    Tempest::Log::e("unable to load landscape mesh");
    throw;
    }

  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  {
  PackedMesh vmesh(*worldMesh,PackedMesh::PK_VisualLnd);
  wview.reset   (new WorldView(*this,vmesh));
  }

  loadProgress(50);
  wdynamic.reset(new DynamicWorld(*this,*worldMesh));
  loadProgress(70);

  globFx.reset(new GlobalEffects(*this));

  wmatrix.reset(new WayMatrix(*this,world.waynet));
  if(1){
    for(auto& vob:world.rootVobs)
      wobj.addRoot(std::move(vob),startup);
    }
  wmatrix->buildIndex();
  bsp = std::move(world.bspTree);
  bspSectors.resize(bsp.sectors.size());
  loadProgress(100);
  }

World::~World() {
  }

void World::createPlayer(std::string_view cls) {
  npcPlayer = addNpc(cls,wmatrix->startPoint().name.c_str());
  if(npcPlayer!=nullptr) {
    npcPlayer->setProcessPolicy(Npc::ProcessPolicy::Player);
    game.script()->setInstanceNPC("HERO",*npcPlayer);
    }
  }

void World::insertPlayer(std::unique_ptr<Npc> &&npc, std::string_view waypoint) {
  if(npc==nullptr)
    return;
  npcPlayer = wobj.insertPlayer(std::move(npc),waypoint);
  if(npcPlayer!=nullptr)
    game.script()->setInstanceNPC("HERO",*npcPlayer);
  }

void World::setPlayer(Npc* npc) {
  if(npc==nullptr)
    return;
  npcPlayer->setProcessPolicy(Npc::ProcessPolicy::AiNormal);
  if (!npcPlayer->isDead()) {
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

  // game.script()->inserNpc("Snapper",wmatrix->startPoint().name.c_str());
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

  fout.write(uint32_t(bspSectors.size()));
  for(size_t i=0;i<bspSectors.size();++i) {
    fout.write(bsp.sectors[i].name,bspSectors[i].guild);
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

GlobalFx World::addGlobalEffect(const Daedalus::ZString& what, uint64_t len, const Daedalus::ZString* argv, size_t argc) {
  return globFx->startEffect(what,len,argv,argc);
  }

MeshObjects::Mesh World::addView(std::string_view visual) const {
  return addView(visual,0,0,0);
  }

MeshObjects::Mesh World::addView(std::string_view visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const {
  return view()->addView(visual,headTex,teetTex,bodyColor);
  }

MeshObjects::Mesh World::addView(const Daedalus::GEngineClasses::C_Item& itm) {
  return view()->addView(itm.visual.c_str(),itm.material,0,itm.material);
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

MeshObjects::Mesh World::addDecalView(const ZenLoad::zCVobData& vob) {
  return view()->addDecalView(vob);
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

World::BspSector* World::portalAt(std::string_view tag) {
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

Focus World::validateFocus(const Focus &def) {
  Focus ret = def;
  ret.npc         = wobj.validateNpc(ret.npc);
  ret.interactive = wobj.validateInteractive(ret.interactive);
  ret.item        = wobj.validateItem(ret.item);

  return ret;
  }

Focus World::findFocus(const Npc &pl, const Focus& def) {
  auto  opt    = WorldObjects::NoFlg;
  auto  coll   = TARGET_COLLECT_FOCUS;
  auto& policy = searchPolicy(pl,coll,opt);

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

bool World::testFocusNpc(Npc* def) {
  auto  opt    = WorldObjects::NoFlg;
  auto  coll   = TARGET_COLLECT_FOCUS;
  auto& policy = searchPolicy(*npcPlayer,coll,opt);

  WorldObjects::SearchOpt optNpc{policy.npc_range1,  policy.npc_range2,  policy.npc_azi,  coll, opt};
  return wobj.testFocusNpc(*npcPlayer,def,optNpc);
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

void World::enableCollizionZone(CollisionZone& z) {
  wobj.enableCollizionZone(z);
  }

void World::disableCollizionZone(CollisionZone& z) {
  wobj.disableCollizionZone(z);
  }

void World::triggerChangeWorld(const std::string& world, const std::string& wayPoint) {
  game.changeWorld(world,wayPoint);
  }

void World::setMobRoutine(gtime time, const Daedalus::ZString& scheme, int32_t state) {
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

void World::aiOutputSound(Npc &player, const std::string &msg) {
  wsound.aiOutput(player.position(),msg);
  }

bool World::aiIsDlgFinished() {
  return game.aiIsDlgFinished();
  }

bool World::isTargeted(Npc& npc) {
  return wobj.isTargeted(npc);
  }

Npc *World::addNpc(std::string_view name, std::string_view at) {
  size_t id = script().getSymbolIndex(name);
  if(id==size_t(-1))
    return nullptr;
  return wobj.addNpc(id,at);
  }

Npc *World::addNpc(size_t npcInstance, std::string_view at) {
  return wobj.addNpc(npcInstance,at);
  }

Npc* World::addNpc(size_t itemInstance, const Tempest::Vec3& at) {
  return wobj.addNpc(itemInstance,at);
  }

Item *World::addItem(size_t itemInstance, std::string_view at) {
  return wobj.addItem(itemInstance,at);
  }

Item* World::addItem(const ZenLoad::zCVobData& vob) {
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

size_t World::hasItems(const char* tag, size_t itemCls) {
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
    auto  tgPos = target->position();
    tgPos.y = target->centerY();

    dir = tgPos-pos;

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

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,perc);
  }

void World::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item &item, int32_t perc) {
  wobj.sendPassivePerc(self,other,victum,item,perc);
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

Sound World::addLandHitEffect(ItemMaterial src, ZenLoad::MaterialGroup reciver, const Tempest::Matrix4x4& pos) {
  // IHI - item hits item
  // IHL - Item hits Level
  return addHitEffect(materialTag(src),materialTag(reciver),"IHL",pos);
  }

Sound World::addWeaponBlkEffect(ItemMaterial src, ItemMaterial reciver, const Tempest::Matrix4x4& pos) {
  // IAI - item attacks item
  return addHitEffect(materialTag(src),materialTag(reciver),"IAI",pos);
  }

Sound World::addHitEffect(std::string_view src, std::string_view dst, std::string_view scheme, const Tempest::Matrix4x4& pos) {
  char buf[128]={};
  std::snprintf(buf,sizeof(buf),"CS_%.*s_%.*s_%.*s",int(scheme.size()),scheme.data(), int(src.size()),src.data(), int(dst.size()),dst.data());

  Tempest::Vec3 pos3;
  pos.project(pos3);

  auto ret = Sound(*this,::Sound::T_Regular,buf,pos3,2500.f,false);

  std::snprintf(buf,sizeof(buf),"CPFX_%.*s_%.*s_%.*s", int(scheme.size()),scheme.data(), int(src.size()),src.data(), int(dst.size()),dst.data());
  if(Gothic::inst().loadParticleFx(buf,true)==nullptr) {
    if(dst=="ME")
      std::snprintf(buf,sizeof(buf),"CPFX_%.*s_%s",int(scheme.size()),scheme.data(),"METAL");
    else if(dst=="WO")
      std::snprintf(buf,sizeof(buf),"CPFX_%.*s_%s",int(scheme.size()),scheme.data(),"WOOD");
    else if(dst=="ST")
      std::snprintf(buf,sizeof(buf),"CPFX_%.*s_%s",int(scheme.size()),scheme.data(),"STONE");
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
  auto sfx = wsound.addDlgSound(s,pos.x,pos.y,pos.z,range,timeLen);
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

const Daedalus::GEngineClasses::C_Focus& World::searchPolicy(const Npc& pl, TargetCollect& coll, WorldObjects::SearchFlg& opt) const {
  opt  = WorldObjects::NoFlg;
  coll = TARGET_COLLECT_FOCUS;

  switch(pl.weaponState()) {
    case WeaponState::Fist:
    case WeaponState::W1H:
    case WeaponState::W2H:
      opt = WorldObjects::NoDeath;
      return game.script()->focusMele();
    case WeaponState::Bow:
    case WeaponState::CBow:
      opt = WorldObjects::SearchFlg(WorldObjects::NoDeath | WorldObjects::NoUnconscious);
      return game.script()->focusRange();
    case WeaponState::Mage:{
      if(auto weapon = pl.inventory().activeWeapon()) {
        int32_t id  = weapon->spellId();
        auto&   spl = script().spellDesc(id);
        coll = TargetCollect(spl.targetCollectAlgo);
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
  return wmatrix->findWayPoint(pos,pos,[](const WayPoint&){ return true; });
  }

const WayPoint* World::findWayPoint(const Tempest::Vec3& pos, const std::function<bool(const WayPoint&)>& f) const {
  return wmatrix->findWayPoint(pos,pos,f);
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
    if(!npc.canSeeNpc(wp.x,wp.y+10,wp.z,true))
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
  auto wp  = wmatrix->findFreePoint(pos,name,[cur,&npc](const WayPoint& wp) -> bool {
    if(wp.isLocked() || &wp==cur)
      return false;
    if(!npc.canSeeNpc(wp.x,wp.y+10,wp.z,true))
      return false;
    return true;
    });
  return wp;
  }

const WayPoint *World::findNextPoint(const WayPoint &pos) const {
  return wmatrix->findNextPoint(pos.position());
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
  if(begin && !begin->isFreePoint() && MoveAlgo::isClose(npc.position(),*begin)) {
    return wmatrix->wayTo(*begin,end);
    }

  begin = wmatrix->findWayPoint(p,end.position(),[&npc](const WayPoint &wp) {
    if(!npc.canSeeNpc(wp.x,wp.y+10,wp.z,true))
      return false;
    return true;
    });
  if(begin==nullptr)
    return WayPath();
  if(MoveAlgo::isClose(p,*begin) && begin==&end)
    return WayPath();

  return wmatrix->wayTo(*begin,end);
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
  const std::string& tg = roomAt(pos);
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
      return bspSectors[i].guild;
    }
  return GIL_NONE;
  }
