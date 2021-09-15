#include "worldobjects.h"

#include "game/serialize.h"
#include "graphics/meshobjects.h"
#include "world/triggers/codemaster.h"
#include "world/triggers/triggerscript.h"
#include "world/triggers/triggerlist.h"
#include "world/triggers/triggerworldstart.h"
#include "world/triggers/messagefilter.h"
#include "world/objects/itemtorchburning.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/vob.h"
#include "world/collisionzone.h"
#include "world.h"
#include "utils/workers.h"
#include "utils/dbgpainter.h"

#include <Tempest/Painter>
#include <Tempest/Application>
#include <Tempest/Log>

using namespace Tempest;
using namespace Daedalus::GameState;

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

WorldObjects::SearchOpt::SearchOpt(float rangeMin, float rangeMax, float azi, TargetCollect collectAlgo, WorldObjects::SearchFlg flags)
  :rangeMin(rangeMin),rangeMax(rangeMax),azi(azi),collectAlgo(collectAlgo),flags(flags) {
  }

WorldObjects::WorldObjects(World& owner):owner(owner){
  npcNear.reserve(512);
  }

WorldObjects::~WorldObjects() {
  }

void WorldObjects::load(Serialize &fin) {
  uint32_t sz = uint32_t(npcArr.size());

  fin.read(sz);
  npcArr.clear();
  for(size_t i=0;i<sz;++i)
    npcArr.emplace_back(std::make_unique<Npc>(owner,size_t(-1),""));
  for(auto& i:npcArr)
    i->load(fin);

  fin.read(sz);
  itemArr.clear();
  for(size_t i=0;i<sz;++i){
    auto it = std::make_unique<Item>(owner,fin,Item::T_World);
    itemArr.emplace_back(std::move(it));
    items.add(itemArr.back().get());
    }

  fin.read(sz);
  if(fin.version()>=28 && interactiveObj.size()!=sz)
    throw std::logic_error("inconsistent *.sav vs world");
  for(auto& i:rootVobs)
    i->loadVobTree(fin);
  if(fin.version()>=10) {
    uint32_t sz = 0;
    fin.read(sz);
    triggerEvents.resize(sz);
    for(auto& i:triggerEvents)
      i.load(fin);
    }
  if(fin.version()>=16) {
    uint32_t sz = 0;
    fin.read(sz);
    routines.resize(sz);
    for(auto& i:routines)
      i.load(fin);
    }

  for(auto& i:interactiveObj)
    i->postValidate();
  for(auto& i:npcArr)
    i->postValidate();
  }

void WorldObjects::save(Serialize &fout) {
  uint32_t sz = uint32_t(npcArr.size());
  fout.write(sz);
  for(auto& i:npcArr)
    i->save(fout);

  sz = uint32_t(itemArr.size());
  fout.write(sz);
  for(auto& i:itemArr)
    i->save(fout);

  sz = uint32_t(interactiveObj.size());
  fout.write(sz);
  for(auto& i:rootVobs)
    i->saveVobTree(fout);
  fout.write(uint32_t(triggerEvents.size()));
  for(auto& i:triggerEvents)
    i.save(fout);

  fout.write(uint32_t(routines.size()));
  for(auto& i:routines)
    i.save(fout);
  }

void WorldObjects::tick(uint64_t dt, uint64_t dtPlayer) {
  auto passive=std::move(sndPerc);
  sndPerc.clear();

  std::sort(npcArr.begin(),npcArr.end(),[](std::unique_ptr<Npc>& a, std::unique_ptr<Npc>& b){
    return a->handle()->id<b->handle()->id;
    });
  for(size_t i=0; i<npcArr.size(); ++i) {
    auto& npc = *npcArr[i];
    if(npc.isPlayer())
      npc.tick(dtPlayer); else
      npc.tick(dt);
    }

  for(auto& i:routines) {
    auto s = i.stateByTime(owner.time());
    if(s!=i.curState) {
      setMobState(i.scheme.c_str(),s);
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

  auto pl = owner.player();
  if(pl==nullptr)
    return;

  npcNear.clear();
  const float nearDist = 3000*3000;
  const float farDist  = 6000*6000;

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

  for(auto& ptr:npcArr) {
    Npc& i = *ptr;
    if(i.isPlayer() || i.isDead())
      continue;

    if(i.processPolicy()==Npc::AiNormal) {
      for(auto& r:passive) {
        if(r.self==&i)
          continue;
        float l = i.qDistTo(r.pos.x,r.pos.y,r.pos.z);
        if(r.item!=size_t(-1) && r.other!=nullptr)
          owner.script().setInstanceItem(*r.other,r.item);
        const float range = float(i.handle()->senses_range);
        if(l<range*range && r.other!=nullptr && r.victum!=nullptr) {
          // aproximation of behavior of original G2
          if(!i.isDown() && !i.isPlayer() &&
             i.canSenseNpc(*r.other, true)!=SensesBit::SENSE_NONE &&
             i.canSenseNpc(*r.victum,true,float(r.other->handle()->senses_range))!=SensesBit::SENSE_NONE
            ) {
            i.perceptionProcess(*r.other,r.victum,l,PercType(r.what));
            }
          }
        }
      }

    if(i.percNextTime()>owner.tickCount())
      continue;
    i.perceptionProcess(*pl);
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
    Log::e("inserNpc: invalid waypoint");

  Npc* npc = new Npc(owner,npcInstance,at);
  if(pos!=nullptr && pos->isLocked()){
    auto p = owner.findNextPoint(*pos);
    if(p)
      pos=p;
    }

  if(pos!=nullptr) {
    npc->setPosition  (pos->x,pos->y,pos->z);
    npc->setDirection (pos->dirX,pos->dirY,pos->dirZ);
    npc->attachToPoint(pos);
    npc->updateTransform();
    npcArr.emplace_back(npc);
    } else {
    npcInvalid.emplace_back(npc);
    }

  return npc;
  }

Npc* WorldObjects::addNpc(size_t npcInstance, const Vec3& pos) {
  Npc* npc = new Npc(owner,npcInstance,"");
  npc->setPosition  (pos.x,pos.y,pos.z);
  //npc->setDirection (pos->dirX,pos->dirY,pos->dirZ);
  npc->updateTransform();

  npcArr.emplace_back(npc);
  return npc;
  }

Npc* WorldObjects::insertPlayer(std::unique_ptr<Npc> &&npc, std::string_view at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr){
    Log::e("insertPlayer: invalid waypoint");
    return nullptr;
    }

  if(pos!=nullptr && pos->isLocked()){
    auto p = owner.findNextPoint(*pos);
    if(p)
      pos=p;
    }
  if(pos!=nullptr) {
    npc->setPosition  (pos->x,pos->y,pos->z);
    npc->setDirection (pos->dirX,pos->dirY,pos->dirZ);
    npc->attachToPoint(pos);
    npc->updateTransform();
    }
  npcArr.emplace_back(std::move(npc));
  return npcArr.back().get();
  }

std::unique_ptr<Npc> WorldObjects::takeNpc(const Npc* ptr) {
  for(size_t i=0; i<npcArr.size(); ++i){
    auto& npc=*npcArr[i];
    if(&npc==ptr){
      auto ret=std::move(npcArr[i]);
      npcArr[i] = std::move(npcArr.back());
      npcArr.pop_back();
      return ret;
      }
    }
  return nullptr;
  }

void WorldObjects::tickNear(uint64_t /*dt*/) {
  for(Npc* i:npcNear) {
    auto pos=i->position();
    for(CollisionZone* z:collisionZn)
      if(z->checkPos(pos + Vec3(0,i->translateY(),0)))
        z->onIntersect(*i);
    }
  }

void WorldObjects::tickTriggers(uint64_t /*dt*/) {
  auto evt = std::move(triggerEvents);
  triggerEvents.clear();

  for(auto& e:evt)
    execTriggerEvent(e);
  }

void WorldObjects::triggerEvent(const TriggerEvent &e) {
  triggerEvents.push_back(e);
  }

void WorldObjects::execTriggerEvent(const TriggerEvent& e) {
  if(e.timeBarrier>owner.tickCount()) {
    triggerEvent(std::move(e));
    return;
    }

  bool emitted=false;
  for(auto& i:triggers) {
    auto& t = *i;
    if(t.name()==e.target) { // NOTE: trigger name is not unique - more then one trigger can be activated
      t.processEvent(e);
      emitted=true;
      }
    }
  if(!emitted)
    Log::d("unable to process trigger: \"",e.target,"\"");
  }

void WorldObjects::updateAnimation(uint64_t dt) {
  static bool doAnim=true;
  if(!doAnim)
    return;
  Workers::parallelFor(npcArr,[dt](std::unique_ptr<Npc>& i){
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
  if(!npc.isAtack())
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

Npc *WorldObjects::findNpcByInstance(size_t instance) {
  for(auto& i:npcArr)
    if(i->handle()->instanceSymbol==instance)
      return i.get();
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
  if(tg->hasVolume())
    triggersZn.emplace_back(tg);
  triggers.emplace_back(tg);
  }

void WorldObjects::triggerOnStart(bool firstTime) {
  TriggerEvent evt("","",firstTime ? TriggerEvent::T_StartupFirstTime : TriggerEvent::T_Startup);
  for(auto& i:triggers)
    i->processOnStart(evt);
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

Item* WorldObjects::addItem(const ZenLoad::zCVobData &vob) {
  size_t inst = owner.script().getSymbolIndex(vob.oCItem.instanceName);
  Item*  it   = addItem(inst,"");
  if(it==nullptr)
    return nullptr;

  Matrix4x4 m { vob.worldMatrix.mv };
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

  const float l = dir.length();
  b.setDirection(dir*speed/l);
  b.setTargetRange(tgRange);
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
  //size_t ItLsTorchburned  = owner.script().getSymbolIndex("ItLsTorchburned");
  size_t ItLsTorchburning = owner.script().getSymbolIndex("ItLsTorchburning");

  if(itemInstance==size_t(-1))
    return nullptr;

  std::unique_ptr<Item> ptr;
  if(itemInstance==ItLsTorchburning)
    ptr.reset(new ItemTorchBurning(owner,itemInstance,Item::T_WorldDyn)); else
    ptr.reset(new Item(owner,itemInstance,Item::T_WorldDyn));

  auto* it=ptr.get();
  it->handle().owner = ownerNpc==size_t(-1) ? 0 : uint32_t(ownerNpc);
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

void WorldObjects::addRoot(ZenLoad::zCVobData&& vob, bool startup) {
  auto p = Vob::load(nullptr,owner,std::move(vob),startup);
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

Interactive* WorldObjects::findInteractive(const Npc &pl, Interactive* def, const SearchOpt& opt) {
  def = validateInteractive(def);
  if(def && testObj(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
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

Npc* WorldObjects::findNpc(const Npc &pl, Npc *def, const SearchOpt& opt) {
  def = validateNpc(def);
  if(def) {
    auto xopt  = opt;
    xopt.flags = SearchFlg(xopt.flags | SearchFlg::NoAngle | SearchFlg::NoRay);
    if(def && testObj(*def,pl,xopt))
      return def;
    }
  auto r = findObj(npcArr,pl,opt);
  return r ? r->get() : nullptr;
  }

Item *WorldObjects::findItem(const Npc &pl, Item *def, const SearchOpt& opt) {
  def = validateItem(def);
  if(def && testObj(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
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
  for(auto& i:interactiveObj){
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

Interactive *WorldObjects::aviableMob(const Npc &pl, const char* dest) {
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

void WorldObjects::setMobRoutine(gtime time, const Daedalus::ZString& scheme, int32_t state) {
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

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.pos    = self.position();
  m.self   = &self;
  m.other  = &other;
  m.victum = &victum;

  sndPerc.push_back(m);
  }

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item &itm, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.pos    = self.position();
  m.self   = &self;
  m.other  = &other;
  m.victum = &victum;
  m.item   = itm.handle().instanceSymbol;

  sndPerc.push_back(m);
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

      auto& npc = *npcInvalid.back();
      npc.attachToPoint(nullptr);
      npc.setPosition(-1000,-1000,-1000); // FIXME
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
      if(i->schemeName()==r.scheme.c_str())
        state = r.curState;
      }
    i->resetPositionToTA(state);
    }
  }

void WorldObjects::setMobState(const char* scheme, int32_t st) {
  for(auto& i:rootVobs)
    i->setMobState(scheme,st);
  }

template<class T>
T& deref(std::unique_ptr<T>& x){ return *x; }

template<class T>
T& deref(T& x){ return x; }

template<class T>
bool checkFlag(T&,WorldObjects::SearchFlg){ return true; }

static bool checkFlag(Npc& n,WorldObjects::SearchFlg f){
  if(n.handle()->noFocus)
    return false;
  if(bool(f&WorldObjects::NoDeath) && n.isDead())
    return false;
  if(bool(f&WorldObjects::NoUnconscious) && n.isUnconscious())
    return false;
  return true;
  }

static bool checkFlag(Interactive& i,WorldObjects::SearchFlg f){
  if(bool(f&WorldObjects::FcOverride)!=i.overrideFocus())
    return false;
  return true;
  }

template<class T>
bool canSee(const Npc&,const T&){ return true; }

static bool canSee(const Npc& pl,const Npc& n){
  return pl.canSeeNpc(n,true);
  }

static bool canSee(const Npc& pl,const Interactive& n){
  return n.canSeeNpc(pl,true);
  }

static bool canSee(const Npc& pl,const Item& n){
  auto p0 = pl.position();
  auto p1 = n.position();
  const float plY = p0.y;
  const float itY = p1.y;
  if(plY<=itY && itY<=plY+180)
    return pl.canSeeNpc(p1.x,plY+180,p1.z,true);
  return pl.canSeeNpc(p1.x,itY+20,p1.z,true);
  }

template<class T>
auto WorldObjects::findObj(T &src,const Npc &pl, const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type* {
  typename std::remove_reference<decltype(src[0])>::type* ret=nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  if(owner.view()==nullptr)
    return nullptr;

  if(opt.collectAlgo==TARGET_COLLECT_NONE || opt.collectAlgo==TARGET_COLLECT_CASTER)
    return nullptr;

  for(auto& n:src){
    float nlen = rlen;
    if(testObj(n,pl,opt,nlen)){
      rlen = nlen;
      ret=&n;
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

  auto pos  = npc.position();
  auto dpos = pl.position()-pos;
  //float dx=pl.position()[0]-pos[0];
  //float dy=pl.position()[1]-pos[1];
  //float dz=pl.position()[2]-pos[2];

  float l = dpos.quadLength();
  if(l>qmax || l<qmin)
    return false;

  auto angle=std::atan2(dpos.z,dpos.x);
  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  l = std::sqrt(l);
  if(l<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
    rlen=l;
    return true;
    }
  return false;
  }
