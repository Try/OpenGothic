#include "worldobjects.h"

#include "game/serialize.h"
#include "world/objects/itemtorchburning.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/vob.h"
#include "world/collisionzone.h"
#include "world/triggers/pfxcontroller.h"
#include "world/triggers/triggerworldstart.h"
#include "world/triggers/abstracttrigger.h"
#include "world.h"
#include "utils/workers.h"
#include "utils/dbgpainter.h"
#include "gothic.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/Log>

#include <glm/gtc/type_ptr.hpp>

using namespace Tempest;

int32_t WorldObjects::MobStates::stateByTime(gtime t) const {
  t = t.timeInDay();
  for(size_t i=routines.size(); i>0; ) {
    --i;
    if(routines[i].time<=t) {
      return routines[i].state;
      }
    }
  if(routines.size()>0)
    return routines.back().state;
  return 0;
  }

void WorldObjects::MobStates::save(Serialize& fout) {
  fout.write(curState,scheme);
  fout.write(uint32_t(routines.size()));
  for(auto& i:routines) {
    fout.write(i.time,i.state);
    }
  }

void WorldObjects::MobStates::load(Serialize& fin) {
  fin.read(curState,scheme);
  uint32_t sz=0;
  fin.read(sz);
  routines.resize(sz);
  for(auto& i:routines) {
    fin.read(i.time,i.state);
    }
  }

WorldObjects::SearchOpt::SearchOpt(float rangeMin, float rangeMax, float azi, TargetCollect collectAlgo, TargetType collectType, WorldObjects::SearchFlg flags)
  :rangeMin(rangeMin),rangeMax(rangeMax),azi(azi),collectAlgo(collectAlgo),collectType(collectType),flags(flags) {
  }

WorldObjects::WorldObjects(World& owner):owner(owner){
  npcNear.reserve(512);
  }

WorldObjects::~WorldObjects() {
  }

void WorldObjects::load(Serialize &fin) {
  {
  uint16_t v = 0;
  fin.setEntry("worlds/",fin.worldName(),"/version");
  fin.read(v);
  fin.setVersion(v);
  }
  itemArr.clear();
  items.clear();

  uint32_t sz = fin.directorySize("worlds/",fin.worldName(),"/npc/");
  npcArr.resize(sz);
  for(size_t i=0; i<sz; ++i)
    npcArr[i] = std::make_unique<Npc>(owner,size_t(-1),"");
  for(size_t i=0; i<npcArr.size(); ++i) {
    npcArr[i]->load(fin,i);
    }

  fin.setEntry("worlds/",fin.worldName(),"/items");
  fin.read(sz);
  for(size_t i=0; i<sz; ++i) {
    auto it = std::make_unique<Item>(owner,fin,Item::T_World);
    itemArr.emplace_back(std::move(it));
    items.add(itemArr.back().get());
    }

  for(auto& i:rootVobs)
    i->loadVobTree(fin);

  fin.setEntry("worlds/",fin.worldName(),"/triggerEvents");
  fin.read(sz);
  triggerEvents.resize(sz);
  for(auto& i:triggerEvents)
    i.load(fin);

  fin.setEntry("worlds/",fin.worldName(),"/routines");
  fin.read(sz);
  routines.resize(sz);
  for(auto& i:routines)
    i.load(fin);

  for(auto& i:interactiveObj)
    i->postValidate();
  for(auto& i:npcArr)
    i->postValidate();
  }

void WorldObjects::save(Serialize &fout) {
  fout.setEntry("worlds/",fout.worldName(),"/version");
  fout.write(Serialize::Version::Current);

  for(size_t i=0; i<npcArr.size(); ++i) {
    npcArr[i]->save(fout,i);
    }

  fout.setEntry("worlds/",fout.worldName(),"/items");
  uint32_t sz = uint32_t(itemArr.size());
  fout.write(sz);
  for(auto& i:itemArr)
    i->save(fout);

  fout.setEntry("worlds/",fout.worldName(),"/mobsi");
  for(auto& i:rootVobs)
    i->saveVobTree(fout);

  fout.setEntry("worlds/",fout.worldName(),"/triggerEvents");
  fout.write(uint32_t(triggerEvents.size()));
  for(auto& i:triggerEvents)
    i.save(fout);

  fout.setEntry("worlds/",fout.worldName(),"/routines");
  fout.write(uint32_t(routines.size()));
  for(auto& i:routines)
    i.save(fout);
  }

void WorldObjects::tick(uint64_t dt, uint64_t dtPlayer) {
  auto passive=std::move(sndPerc);
  sndPerc.clear();

  bool needSort = false;
  for(size_t i=1; i<npcArr.size(); ++i) {
    auto& a = npcArr[i-1];
    auto& b = npcArr[i-0];
    if(a->handle().id>b->handle().id) {
      needSort = true;
      break;
      }
    }

  if(needSort) {
    std::sort(npcArr.begin(),npcArr.end(),[](std::unique_ptr<Npc>& a, std::unique_ptr<Npc>& b){
      return a->handle().id<b->handle().id;
      });
    }

  auto       camera  = Gothic::inst().camera();
  const bool freeCam = (camera!=nullptr && camera->isFree());
  const auto pl      = owner.player();
  for(size_t i=0; i<npcArr.size(); ++i) {
    auto& npc = *npcArr[i];
    uint64_t d = (pl==&npc ? dtPlayer : dt);
    if(freeCam && pl==&npc)
      continue;
    npc.tick(d);
    }

  for(auto& i:routines) {
    auto s = i.stateByTime(owner.time());
    if(s!=i.curState) {
      setMobState(i.scheme,s);
      i.curState = s;
      }
    }

  for(auto& i:interactiveObj)
    i->tick(dt);

  for(auto i:triggersTk)
    i->tick(dt);

  bullets.remove_if([](Bullet& b){
    return b.isFinished();
    });

  for(size_t i=0; i<effects.size();) {
    if(effects[i].timeUntil<owner.tickCount()) {
      effects[i].eff.setActive(false);
      effects[i] = std::move(effects.back());
      effects.pop_back();
      } else {
      effects[i].eff.tick(dt);
      ++i;
      }
    }

  if(pl==nullptr)
    return;

  npcNear.clear();
  //const int   PERC_DIST_INTERMEDIAT = 1000;
  const float nearDist              = 3000*3000;
  const float farDist               = 6000*6000;

  auto plPos = pl->position();
  for(auto& i:npcArr) {
    float dist = (i->position()-plPos).quadLength();
    if(dist<nearDist){
      npcNear.push_back(i.get());
      if(i.get()!=pl)
        i->setProcessPolicy(Npc::ProcessPolicy::AiNormal);
      } else
    if(dist<farDist) {
      i->setProcessPolicy(Npc::ProcessPolicy::AiFar);
      } else {
      i->setProcessPolicy(Npc::ProcessPolicy::AiFar2);
      }
    }
  tickNear(dt);
  for(CollisionZone* z:collisionZn)
    z->tick(dt);
  tickTriggers(dt);

  for(auto& ptr:npcNear) {
    Npc& i = *ptr;
    if(i.isPlayer() || i.isDead())
      continue;

    const uint64_t percNextTime = i.percNextTime();
    if(percNextTime<=owner.tickCount()) {
      i.perceptionProcess(*pl);
      }

    if(i.processPolicy()==Npc::AiNormal) {
      for(auto& r:passive)
        passivePerceptionProcess(r, *ptr, *pl);
      }
    }
  }

uint32_t WorldObjects::npcId(const Npc *ptr) const {
  if(ptr==nullptr)
    return uint32_t(-1);
  for(size_t i=0;i<npcArr.size();++i)
    if(npcArr[i].get()==ptr)
      return uint32_t(i);
  return uint32_t(-1);
  }

uint32_t WorldObjects::itmId(const void *ptr) const {
  for(size_t i=0;i<itemArr.size();++i)
    if(&itemArr[i]->handle()==ptr)
      return uint32_t(i);
  return uint32_t(-1);
  }

uint32_t WorldObjects::mobsiId(const void* ptr) const {
  uint32_t ret=0;
  for(auto& i:interactiveObj) {
    if(i==ptr)
      return ret;
    ++ret;
    }
  return uint32_t(-1);
  }

Npc* WorldObjects::addNpc(size_t npcInstance, std::string_view at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr)
    Log::e("addNpc: invalid waypoint");

  Npc* npc = new Npc(owner,npcInstance,at);
  if(pos!=nullptr && pos->isLocked()){
    auto p = owner.findNextPoint(*pos);
    if(p)
      pos=p;
    }

  bool valid = false;
  if(pos!=nullptr) {
    valid = true;
    }
  if(npc->resetPositionToTA()) {
    valid = true;
    }

  if(valid) {
    if(auto p = npc->currentWayPoint())
      pos = p;
    if(pos==nullptr)
      pos = &owner.deadPoint();
    npc->setPosition  (pos->x,pos->y,pos->z);
    npc->setDirection (pos->dirX,pos->dirY,pos->dirZ);
    npc->attachToPoint(pos);
    npc->updateTransform();
    npcArr.emplace_back(npc);
    } else {
    auto& point = owner.deadPoint();
    npc->attachToPoint(nullptr);
    npc->setPosition(point.position());
    npc->updateTransform();
    npcInvalid.emplace_back(npc);
    }

  return npc;
  }

Npc* WorldObjects::addNpc(size_t npcInstance, const Vec3& pos) {
  auto point = owner.findWayPoint(pos);
  if(point==nullptr) {
    point = owner.findFreePoint(pos, "");
    }

  auto pstr  = point==nullptr ? "" : point->name; // vanilla assign some point to all npc's
  Npc* npc   = new Npc(owner,npcInstance,pstr);
  npc->setPosition  (pos.x,pos.y,pos.z);
  npc->updateTransform();
  owner.script().invokeRefreshAtInsert(*npc);

  npcArr.emplace_back(npc);
  return npc;
  }

Npc* WorldObjects::insertPlayer(std::unique_ptr<Npc> &&npc, std::string_view at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr) {
    Log::e("insertPlayer: invalid waypoint, using fallback");
    // freemine.zen
    pos = &owner.startPoint();
    }

  if(pos->isLocked()) {
    auto p = owner.findNextPoint(*pos);
    if(p)
      pos=p;
    }
  npc->setPosition  (pos->x,pos->y,pos->z);
  npc->setDirection (pos->dirX,pos->dirY,pos->dirZ);
  npc->attachToPoint(pos);
  npc->updateTransform();
  npcArr.emplace_back(std::move(npc));
  return npcArr.back().get();
  }

std::unique_ptr<Npc> WorldObjects::takeNpc(const Npc* ptr) {
  for(size_t i=0; i<npcArr.size(); ++i){
    auto& npc=*npcArr[i];
    if(&npc==ptr){
      auto ret=std::move(npcArr[i]);
      npcArr.erase(npcArr.begin() + int32_t(i));
      return ret;
      }
    }
  return nullptr;
  }

void WorldObjects::removeNpc(Npc& npc) {
  auto ptr = takeNpc(&npc);
  if(ptr==nullptr)
    return;
  auto& point = owner.deadPoint();
  npc.attachToPoint(nullptr);
  npc.setPosition(point.position());
  npc.updateTransform();
  npcRemoved.emplace_back(std::move(ptr));
  }

void WorldObjects::tickNear(uint64_t /*dt*/) {
  for(Npc* i:npcNear) {
    auto pos = i->position() + Vec3(0,i->translateY(),0);
    for(CollisionZone* z:collisionZn)
      if(z->checkPos(pos))
        z->onIntersect(*i);
    }
  }

void WorldObjects::triggerEvent(const TriggerEvent &e) {
  triggerEvents.push_back(e);
  }

void WorldObjects::tickTriggers(uint64_t /*dt*/) {
  execDelayedEvents();

  auto evt = std::move(triggerEvents);
  triggerEvents.clear();

  for(auto& e:evt)
    owner.execTriggerEvent(e);
  }

void WorldObjects::execDelayedEvents() {
  auto def = std::move(triggersDef);
  triggersDef.clear();
  for(auto i:def) {
    i->processDelayedEvents();
    if(i->hasDelayedEvents())
      triggersDef.push_back(i);
    }
  }

bool WorldObjects::execTriggerEvent(const TriggerEvent& e) {
  bool emitted=false;

  for(auto& i:triggers) {
    auto& t = *i;
    if(t.name()!=e.target)
      continue; // NOTE: trigger name is not unique - more then one trigger can be activated
    t.processEvent(e);
    emitted = true;
    }

  return emitted;
  }

void WorldObjects::updateAnimation(uint64_t dt) {
  static bool doAnim=true;
  if(!doAnim)
    return;
  if(dt==0)
    return;
  Workers::parallelTasks(npcArr,[dt](std::unique_ptr<Npc>& i){
    i->updateAnimation(dt);
    });
  interactiveObj.parallelFor([dt](Interactive& i){
    i.updateAnimation(dt);
    });
  }

bool WorldObjects::isTargeted(Npc& dst) {
  std::atomic_flag flg = ATOMIC_FLAG_INIT;
  Workers::parallelFor(npcArr,[&dst,&flg](std::unique_ptr<Npc>& i) {
    if(isTargetedBy(*i,dst))
      flg.test_and_set();
    });
  return flg.test_and_set();
  }

bool WorldObjects::isTargetedBy(Npc& npc, Npc& dst) {
  if(npc.target()!=&dst)
    return false;
  if(npc.processPolicy()!=Npc::AiNormal || npc.weaponState()==WeaponState::NoWeapon)
    return false;
  if(!npc.isAttack())
    return false;
  return true;
  }

Npc *WorldObjects::findHero() {
  for(auto& i:npcArr){
    if(i->processPolicy()==Npc::ProcessPolicy::Player)
      return i.get();
    }
  return nullptr;
  }

Npc *WorldObjects::findNpcByInstance(size_t instance, size_t n) {
  for(auto& i:npcArr) {
    if(i->handle().symbol_index()==instance) {
      if(n==0)
        return i.get();
      --n;
      }
    }
  return nullptr;
  }

Item* WorldObjects::findItemByInstance(size_t instance, size_t n) {
  for(auto& i:itemArr) {
    if(i->handle().symbol_index()==instance) {
      if(n==0)
        return i.get();
      --n;
      }
    }
  return nullptr;
  }

void WorldObjects::detectNpcNear(const std::function<void(Npc&)>& f) {
  for(auto& i:npcNear)
    f(*i);
  }

void WorldObjects::detectNpc(const float x, const float y, const float z,
                             const float r, const std::function<void(Npc&)>& f) {
  float maxDist=r*r;
  for(auto& i:npcArr) {
    auto qDist = (i->position()-Vec3(x,y,z)).quadLength();
    if(qDist<maxDist)
      f(*i);
    }
  }

void WorldObjects::detectItem(const float x, const float y, const float z,
                              const float r, const std::function<void(Item&)>& f) {
  float maxDist=r*r;
  for(auto& i:itemArr) {
    auto qDist = (i->position()-Vec3(x,y,z)).quadLength();
    if(qDist<maxDist)
      f(*i);
    }
  }

void WorldObjects::addTrigger(AbstractTrigger* tg) {
  triggers.emplace_back(tg);
  }

void WorldObjects::enableDefTrigger(AbstractTrigger& t) {
  for(auto& i:triggersDef)
    if(i==&t)
      return;
  triggersDef.push_back(&t);
  }

bool WorldObjects::triggerOnStart(bool firstTime) {
  bool ret = false;
  TriggerEvent evt("","",firstTime ? TriggerEvent::T_StartupFirstTime : TriggerEvent::T_Startup);
  for(auto& i:triggers) {
    if(auto ts = dynamic_cast<TriggerWorldStart*>(i)) {
      ts->processEvent(evt);
      ret = true;
      }
    }
  return ret;
  }

void WorldObjects::enableTicks(AbstractTrigger& t) {
  for(auto& i:triggersTk)
    if(i==&t)
      return;
  triggersTk.push_back(&t);
  }

void WorldObjects::disableTicks(AbstractTrigger& t) {
  for(auto& i:triggersTk)
    if(i==&t) {
      i = triggersTk.back();
      triggersTk.pop_back();
      return;
      }
  }

void WorldObjects::setCurrentCs(CsCamera* cs) {
  currentCsCamera = cs;
  }

CsCamera* WorldObjects::currentCs() const {
  return currentCsCamera;
  }

void WorldObjects::enableCollizionZone(CollisionZone& z) {
  collisionZn.push_back(&z);
  }

void WorldObjects::disableCollizionZone(CollisionZone& z) {
  for(auto& i:collisionZn)
    if(i==&z) {
      i = collisionZn.back();
      collisionZn.pop_back();
      return;
      }
  }

void WorldObjects::runEffect(Effect&& ex) {
  ex.setBullet(nullptr,owner);

  EffectState e;
  e.eff       = std::move(ex);
  e.timeUntil = owner.tickCount()+e.eff.effectPrefferedTime();
  effects.push_back(std::move(e));
  }

void WorldObjects::stopEffect(const VisualFx& vfx) {
  for(auto& i:npcArr)
    i->stopEffect(vfx);
  }

Item* WorldObjects::addItem(const zenkit::VItem& vob) {
  size_t inst = owner.script().findSymbolIndex(vob.instance);
  Item*  it   = addItem(inst,"");
  if(it==nullptr)
    return nullptr;

  glm::mat4x4 worldMatrix = vob.rotation;
  worldMatrix[3] = glm::vec4(vob.position, 1);

  Matrix4x4 m { glm::value_ptr(worldMatrix) };
  it->setObjMatrix(m);
  return it;
  }

std::unique_ptr<Item> WorldObjects::takeItem(Item &it) {
  for(auto& i:itemArr)
    if(i.get()==&it){
      auto ret=std::move(i);
      i = std::move(itemArr.back());
      itemArr.pop_back();
      items.del(ret.get());
      ret->setPhysicsDisable();
      onItemRemoved(*ret);
      return ret;
      }
  return nullptr;
  }

void WorldObjects::removeItem(Item &it) {
  if(auto ptr=takeItem(it)){
    (void)ptr;
    }
  }

size_t WorldObjects::hasItems(std::string_view tag, size_t itemCls) {
  for(auto& i:interactiveObj)
    if(i->tag()==tag) {
      return i->inventory().itemCount(itemCls);
      }
  return 0;
  }

void WorldObjects::onItemRemoved(const Item& itm) {
  for(auto& i:npcArr)
    i->onWldItemRemoved(itm);
  owner.script().onWldItemRemoved(itm);
  }

Bullet& WorldObjects::shootBullet(const Item& itmId, const Vec3& pos, const Vec3& dir, float tgRange, float speed) {
  bullets.emplace_back(owner,itmId,pos);
  auto& b = bullets.back();

  const float rgnBias = 50.f;
  const float l = dir.length();
  b.setDirection(dir*speed/l);
  b.setTargetRange(tgRange + rgnBias);
  return b;
  }

Item *WorldObjects::addItem(size_t itemInstance, std::string_view at) {
  Item* item = nullptr;
  Tempest::Vec3 pos;
  Tempest::Vec3 dir;
  const WayPoint* waypoint = owner.findPoint(at);

  if(waypoint != nullptr) {
    pos = {waypoint->x, waypoint->y, waypoint->z};
    dir = {waypoint->dirX, waypoint->dirY, waypoint->dirZ};
  }

  item = addItem(itemInstance, pos, dir);

  return item;
  }

Item* WorldObjects::addItem(size_t itemInstance, const Tempest::Vec3& pos) {
  return addItem(itemInstance, pos, {0,0,0});
  }

Item* WorldObjects::addItem(size_t itemInstance, const Tempest::Vec3& pos, const Tempest::Vec3& dir) {
  if(itemInstance==size_t(-1))
    return nullptr;

  std::unique_ptr<Item> ptr{new Item(owner,itemInstance,Item::T_World)};
  auto* it=ptr.get();
  itemArr.emplace_back(std::move(ptr));
  items.add(itemArr.back().get());

  it->setPosition (pos.x, pos.y, pos.z);
  it->setDirection(dir.x, dir.y, dir.z);

  return it;
  }

Item* WorldObjects::addItemDyn(size_t itemInstance, const Tempest::Matrix4x4& pos, size_t ownerNpc) {
  //size_t ItLsTorchburned  = owner.script().findSymbolIndex("ItLsTorchburned");
  size_t ItLsTorchburning = owner.script().findSymbolIndex("ItLsTorchburning");

  if(itemInstance==size_t(-1))
    return nullptr;

  std::unique_ptr<Item> ptr;
  if(itemInstance==ItLsTorchburning)
    ptr.reset(new ItemTorchBurning(owner,itemInstance,Item::T_WorldDyn)); else
    ptr.reset(new Item(owner,itemInstance,Item::T_WorldDyn));

  auto* it=ptr.get();
  it->handle().owner = ownerNpc==size_t(-1) ? 0 : int32_t(ownerNpc);
  itemArr.emplace_back(std::move(ptr));
  items.add(itemArr.back().get());

  it->setObjMatrix(pos);

  return it;
  }

void WorldObjects::addInteractive(Interactive* obj) {
  interactiveObj.add(obj);
  }

void WorldObjects::addStatic(StaticObj* obj) {
  objStatic.push_back(obj);
  }

void WorldObjects::addRoot(const std::shared_ptr<zenkit::VirtualObject>& vob, bool startup) {
  auto p = Vob::load(nullptr,owner,*vob,(startup ? Vob::Startup : Vob::None) | Vob::Static);
  if(p==nullptr)
    return;
  rootVobs.emplace_back(std::move(p));
  }

void WorldObjects::invalidateVobIndex() {
  items.invalidate();
  interactiveObj.invalidate();
  }

Interactive* WorldObjects::validateInteractive(Interactive *def) {
  return interactiveObj.hasObject(def) ? def : nullptr;
  }

Npc *WorldObjects::validateNpc(Npc *def) {
  if(def==nullptr)
    return nullptr;
  for(auto& i:npcArr)
    if(i.get()==def)
      return def;
  return nullptr;
  }

Item *WorldObjects::validateItem(Item *def) {
  return items.hasObject(def) ? def : nullptr;
  }

bool WorldObjects::testFocusNpc(const Npc &pl, Npc* def, const SearchOpt& opt) {
  if(def && testObj(*def,pl,opt))
    return true;
  return false;
  }

Interactive* WorldObjects::findInteractive(const Npc &pl, Interactive* def, const SearchOpt& opt) {
  def = validateInteractive(def);
  if(def && testObj(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
    return nullptr;
  if(!bool(opt.collectType&TARGET_TYPE_ALL))
    return nullptr;

  Interactive* ret  = nullptr;
  float        rlen = opt.rangeMax*opt.rangeMax;
  interactiveObj.find(pl.position(),opt.rangeMax,[&](Interactive& n){
    float nlen = rlen;
    if(testObj(n,pl,opt,nlen)){
      rlen = nlen;
      ret  = &n;
      }
    return false;
    });
  return ret;
  }

Npc* WorldObjects::findNpcNear(const Npc& pl, Npc* def, const SearchOpt& opt) {
  def = validateNpc(def);
  if(def) {
    auto xopt  = opt;
    xopt.flags = SearchFlg(xopt.flags | SearchFlg::NoAngle | SearchFlg::NoRay);
    if(def && testObj(*def,pl,xopt))
      return def;
    }
  auto r = findObj(npcNear,pl,opt);
  if(r!=nullptr && (!Gothic::options().hideFocus || !r->isDead() ||
                     r->inventory().iterator(Inventory::T_Ransack).isValid()))
    return r;
  return nullptr;
  }

Item *WorldObjects::findItem(const Npc &pl, Item *def, const SearchOpt& opt) {
  def = validateItem(def);
  if(def && testObj(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
    return nullptr;
  if(!bool(opt.collectType&(TARGET_TYPE_ALL|TARGET_TYPE_ITEMS)))
    return nullptr;

  Item* ret  = nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  items.find(pl.position(),opt.rangeMax,[&](Item& n){
    float nlen = rlen;
    if(testObj(n,pl,opt,nlen)){
      rlen = nlen;
      ret  = &n;
      }
    return false;
    });
  return ret;
  }

void WorldObjects::marchInteractives(DbgPainter &p) const {
  static bool ddraw=false;
  if(!ddraw)
    return;
  for(auto& i:interactiveObj) {
    auto m = p.mvp;
    m.mul(i->transform());

    float x=0,y=0,z=0;
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = (0.5f*x+0.5f)*float(p.w);
    y = (0.5f*y+0.5f)*float(p.h);

    p.setBrush(Tempest::Color(1,1,1,1));
    p.painter.drawRect(int(x),int(y),1,1);

    i->marchInteractives(p);
    }
  }

Interactive *WorldObjects::availableMob(const Npc &pl, std::string_view dest) {
  const float  dist=100*10.f;
  Interactive* ret =nullptr;

  if(auto i = pl.interactive()){
    if(i->checkMobName(dest))
      return i;
    }

  float curDist=dist*dist;
  interactiveObj.find(pl.position(),dist,[&](Interactive& i){
    if(i.isAvailable() && i.checkMobName(dest)) {
      float d = pl.qDistTo(i);
      if(d<curDist){
        ret    = &i;
        curDist = d;
        }
      }
    return false;
    });

  if(ret==nullptr)
    return nullptr;
  return ret;
  }

void WorldObjects::setMobRoutine(gtime time, std::string_view scheme, int32_t state) {
  MobRoutine r;
  r.time  = time;
  r.state = state;

  for(auto& i:routines) {
    if(i.scheme==scheme) {
      i.routines.push_back(r);
      std::sort(i.routines.begin(),i.routines.end(),[](const MobRoutine& l, const MobRoutine& r){
        return l.time<r.time;
        });
      return;
      }
    }
  MobStates st;
  st.scheme = scheme;
  st.routines.push_back(r);
  routines.emplace_back(std::move(st));
  }

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc* victum, Item* itm, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.pos    = self.position();
  m.self   = &self;
  m.other  = &other;
  m.victum = victum;
  if(itm!=nullptr)
    m.item   = itm->handle().symbol_index();
  sndPerc.push_back(m);
  }

void WorldObjects::sendImmediatePerc(Npc& self, Npc& other, Npc& victum, Item* itm, int32_t perc) {
  const auto pl = owner.player();
  if(pl==nullptr || pl->bodyStateMasked()==BS_SNEAK)
    return;

  PerceptionMsg r;
  r.what   = perc;
  r.pos    = self.position();
  r.self   = &self;
  r.other  = &other;
  r.victum = &victum;
  if(itm!=nullptr)
    r.item   = itm->handle().symbol_index();

  for(auto& ptr:npcNear) {
    passivePerceptionProcess(r, *ptr, *pl);
    }
  }

void WorldObjects::passivePerceptionProcess(PerceptionMsg& msg, Npc& npc, Npc& pl) {
  if(npc.isPlayer() || npc.isDead())
    return;

  if(npc.processPolicy()!=Npc::AiNormal)
    return;

  if(msg.self==&npc)
    return;

  const float distance = npc.qDistTo(msg.pos);
  const float range    = float(owner.script().percRanges().at(PercType(msg.what), npc.handle().senses_range));

  if(distance > range*range)
    return;

  if(npc.isDown() || npc.isPlayer())
    return;

  if(msg.other==nullptr)
    return;

  /*
  // active only
  const bool active = isActivePerception(PercType(msg.what));
  if(active && npc.canSenseNpc(*msg.other, true)==SensesBit::SENSE_NONE) {
    return;
    }

  // approximation of behavior of original G2
  if(active && msg.victum!=nullptr && npc.canSenseNpc(*msg.victum,true,float(msg.other->handle().senses_range))==SensesBit::SENSE_NONE) {
    return;
    }
  */

  if(msg.item!=size_t(-1) && msg.other!=nullptr)
    owner.script().setInstanceItem(*msg.other,msg.item);
  npc.perceptionProcess(*msg.other,msg.victum,distance,PercType(msg.what));
  }

void WorldObjects::resetPositionToTA() {
  for(auto& r:routines)
    r.curState = 0;

  for(auto& i:npcInvalid)
    npcArr.push_back(std::move(i));
  npcInvalid.clear();

  for(size_t i=0;i<npcArr.size();) {
    auto& n = *npcArr[i];
    if(n.resetPositionToTA()){
      ++i;
      } else {
      npcInvalid.emplace_back(std::move(npcArr[i]));
      npcArr.erase(npcArr.begin()+int(i));

      auto& point = owner.deadPoint();
      auto& npc   = *npcInvalid.back();
      npc.attachToPoint(nullptr);
      npc.setPosition(point.position());
      npc.updateTransform();
      }
    }
  for(auto& i:routines) {
    auto s = i.stateByTime(owner.time());
    i.curState = s;
    }
  for(auto& i:interactiveObj) {
    int32_t state = -1;
    for(auto& r:routines) {
      if(i->schemeName()==r.scheme)
        state = r.curState;
      }
    i->resetPositionToTA(state);
    }
  }

void WorldObjects::setMobState(std::string_view scheme, int32_t st) {
  for(auto& i:rootVobs)
    i->setMobState(scheme,st);
  }

template<class T>
static T& deref(std::unique_ptr<T>& x){ return *x; }

template<class T>
static T& deref(T* x){ return *x; }

template<class T>
static T& deref(T& x){ return x; }

template<class T>
static bool checkFlag(T&,WorldObjects::SearchFlg){ return true; }

static bool checkFlag(Npc& n,WorldObjects::SearchFlg f){
  if(n.handle().no_focus)
    return false;
  if(bool(f&WorldObjects::NoDeath) && n.isDead())
    return false;
  if(bool(f&WorldObjects::NoUnconscious) && n.isUnconscious())
    return false;
  return true;
  }

static bool checkFlag(Interactive& i,WorldObjects::SearchFlg f){
  if(bool(f&WorldObjects::FcOverride) && !i.overrideFocus())
    return false;
  return true;
  }

template<class T>
static bool checkTargetType(T&, TargetType) { return true; }

static bool checkTargetType(Npc& n, TargetType t) {
  return n.isTargetableBySpell(t);
  }

template<class T>
static bool canSee(const Npc&,const T&){ return true; }

static bool canSee(const Npc& pl, const Npc& n){
  return pl.canSeeNpc(n,true);
  }

static bool canSee(const Npc& pl, const Interactive& n){
  return n.canSeeNpc(pl,true);
  }

static bool canSee(const Npc& pl, const Item& n){
  return pl.canSeeItem(n,true);
  }

template<class T>
auto WorldObjects::findObj(T &src,const Npc &pl, const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type {
  typename std::remove_reference<decltype(src[0])>::type ret=nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  if(owner.view()==nullptr)
    return nullptr;

  if(opt.collectAlgo==TARGET_COLLECT_NONE || opt.collectAlgo==TARGET_COLLECT_CASTER)
    return nullptr;

  for(auto& n:src){
    float nlen = rlen;
    if(testObj(n,pl,opt,nlen)){
      rlen = nlen;
      ret = n;
      }
    }
  return ret;
  }

template<class T>
bool WorldObjects::testObj(T &src, const Npc &pl, const WorldObjects::SearchOpt &opt) {
  float rlen = opt.rangeMax*opt.rangeMax;
  return testObj(src,pl,opt,rlen);
  }

template<class T>
bool WorldObjects::testObj(T &src, const Npc &pl, const WorldObjects::SearchOpt &opt,float& rlen){
  const float qmax  = opt.rangeMax*opt.rangeMax;
  const float qmin  = opt.rangeMin*opt.rangeMin;
  const float plAng = pl.rotationRad()+float(M_PI/2);
  const float ang   = float(std::cos(double(opt.azi)*M_PI/180.0));

  auto& npc=deref(src);
  if(reinterpret_cast<void*>(&npc)==reinterpret_cast<const void*>(&pl))
    return false;

  if(!checkFlag(npc,opt.flags))
    return false;
  if(!checkTargetType(npc,opt.collectType))
    return false;

  float l = pl.qDistTo(npc);
  if(l>qmax || l<qmin)
    return false;

  auto pos   = npc.position();
  auto dpos  = pl.position()-pos;
  auto angle = std::atan2(dpos.z,dpos.x);

  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  l = std::sqrt(l);
  if(l<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
    rlen=l;
    return true;
    }
  return false;
  }
