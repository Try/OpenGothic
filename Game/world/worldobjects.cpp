#include "worldobjects.h"

#include "game/serialize.h"
#include "graphics/staticobjects.h"
#include "item.h"
#include "npc.h"
#include "world.h"
#include "utils/workers.h"

#include "world/triggers/codemaster.h"
#include "world/triggers/triggerscript.h"
#include "world/triggers/triggerlist.h"
#include "world/triggers/triggerworldstart.h"
#include "world/triggers/messagefilter.h"

#include <Tempest/Painter>
#include <Tempest/Application>

using namespace Tempest;
using namespace Daedalus::GameState;

WorldObjects::WorldObjects(World& owner):owner(owner){
  npcNear.reserve(512);
  }

WorldObjects::~WorldObjects() {
  }

void WorldObjects::load(Serialize &fin) {
  uint32_t sz = npcArr.size();

  fin.read(sz);
  npcArr.clear();
  for(size_t i=0;i<sz;++i)
    npcArr.emplace_back(std::make_unique<Npc>(owner,fin));

  fin.read(sz);
  itemArr.clear();
  for(size_t i=0;i<sz;++i){
    auto it = std::make_unique<Item>(owner,fin);

    auto& h = *it->handle();
    it->setView(owner.getStaticView(h.visual,h.material));
    itemArr.emplace_back(std::move(it));
    }
  }

void WorldObjects::save(Serialize &fout) {
  uint32_t sz = npcArr.size();
  fout.write(sz);
  for(auto& i:npcArr)
    i->save(fout);

  sz = itemArr.size();
  fout.write(sz);
  for(auto& i:itemArr)
    i->save(fout);
  }

void WorldObjects::tick(uint64_t dt) {
  auto passive=std::move(sndPerc);
  sndPerc.clear();

  for(auto& i:npcArr)
    i->tick(dt);

  for(size_t i=0;i<bullets.size();){
    if(bullets[i].flags()&Bullet::Stopped) {
      ++i;
      continue;
      }

    if(bullets[i].tick(dt)){
      bullets[i]=std::move(bullets.back());
      bullets.pop_back();
      } else {
      ++i;
      }
    }

  auto pl = owner.player();
  if(pl==nullptr)
    return;

  npcNear.clear();
  const float nearDist = 3000*3000;
  const float farDist  = 6000*6000;

  for(auto& i:npcArr) {
    float dist = pl->qDistTo(*i);
    if(dist<nearDist){
      if(!i->isDown())
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
  tickTriggers(dt);

  for(auto& i:npcArr) {
    if(i->isPlayer())
      continue;
    for(auto& r:passive) {
      if(r.self==i.get())
        continue;
      float l = i->qDistTo(r.x,r.y,r.z);
      if(r.item!=size_t(-1) && r.other!=nullptr)
        owner.script().setInstanceItem(*r.other,r.item);
      if(l<i->handle()->senses_range*i->handle()->senses_range) {
        // aproximation of behavior of original G2
        if(i->canSenseNpc(*r.other, true)!=SensesBit::SENSE_NONE &&
           i->canSenseNpc(*r.victum,true)!=SensesBit::SENSE_NONE)
          i->perceptionProcess(*r.other,r.victum,l,Npc::PercType(r.what));
        }
      }

    if(i->percNextTime()>owner.tickCount())
      continue;
    float dist = pl->qDistTo(*i);
    i->perceptionProcess(*pl,dist);
    }
  }

uint32_t WorldObjects::npcId(const void *ptr) const {
  for(size_t i=0;i<npcArr.size();++i)
    if(npcArr[i]->handle()==ptr)
      return i;
  return uint32_t(-1);
  }

uint32_t WorldObjects::itmId(const void *ptr) const {
  for(size_t i=0;i<itemArr.size();++i)
    if(itemArr[i]->handle()==ptr)
      return i;
  return uint32_t(-1);
  }

Npc *WorldObjects::addNpc(size_t npcInstance, const char *at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr){
    Log::e("inserNpc: invalid waypoint");
    return nullptr;
    }
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
    }

  npcArr.emplace_back(npc);
  return npc;
  }

Npc* WorldObjects::insertPlayer(std::unique_ptr<Npc> &&npc,const char* at) {
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
  for(Npc* i:npcNear){
    auto pos=i->position();
    for(AbstractTrigger* t:triggersZn)
      if(t->checkPos(pos[0],pos[1]+i->translateY(),pos[2]))
        t->onIntersect(*i);
    }
  }

void WorldObjects::tickTriggers(uint64_t /*dt*/) {
  auto evt = std::move(triggerEvents);
  triggerEvents.clear();

  for(auto& e:evt) {
    bool emitted=false;
    for(auto& i:triggers)
      if(i->name()==e.target) {
        i->onTrigger(e);
        emitted=true;
        }
    if(!emitted)
      Log::d("unable to process trigger: \"",e.target,"\"");
    }
  }

void WorldObjects::updateAnimation() {
  Workers::parallelFor(npcArr,8,[](std::unique_ptr<Npc>& i){
    i->updateTransform();
    i->updateAnimation();
    });
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

void WorldObjects::detectNpcNear(std::function<void (Npc &)> f) {
  for(auto& i:npcNear)
    f(*i);
  }

void WorldObjects::detectNpc(const float x, const float y, const float z,
                             const float r, std::function<void (Npc &)> f) {
  float maxDist=r*r;
  for(auto& i:npcArr) {
    float dx=i->position()[0]-x;
    float dy=i->position()[1]-y;
    float dz=i->position()[2]-z;

    float qDist = dx*dx+dy*dy+dz*dz;
    if(qDist<maxDist)
      f(*i);
    }
  }

void WorldObjects::addTrigger(ZenLoad::zCVobData&& vob) {
  std::unique_ptr<AbstractTrigger> tg;
  switch(vob.vobType) {
    case ZenLoad::zCVobData::VT_zCMover:
      tg.reset(new MoveTrigger(std::move(vob),owner));
      triggersMv.emplace_back(tg.get());
      break;

    case ZenLoad::zCVobData::VT_oCTriggerChangeLevel:
      tg.reset(new ZoneTrigger(std::move(vob),owner));
      break;

    case ZenLoad::zCVobData::VT_zCCodeMaster:
      tg.reset(new CodeMaster(std::move(vob),owner));
      break;

    case ZenLoad::zCVobData::VT_zCTriggerScript:
      tg.reset(new TriggerScript(std::move(vob),owner));
      break;

    case ZenLoad::zCVobData::VT_oCTriggerWorldStart:
      tg.reset(new TriggerWorldStart(std::move(vob),owner));
      break;

    case ZenLoad::zCVobData::VT_zCTriggerList:
      tg.reset(new TriggerList(std::move(vob),owner));
      break;

    case ZenLoad::zCVobData::VT_zCMessageFilter:
      tg.reset(new MessageFilter(std::move(vob),owner));
      break;

    default:
      tg.reset(new Trigger(std::move(vob),owner));
    }
  if(tg->hasVolume())
    triggersZn.emplace_back(tg.get());
  triggers.emplace_back(std::move(tg));
  }

void WorldObjects::triggerEvent(const TriggerEvent &e) {
  triggerEvents.push_back(e);
  }

void WorldObjects::triggerOnStart(bool wrldStartup) {
  for(auto& i:triggers)
    if(i->vobType()==ZenLoad::zCVobData::VT_oCTriggerWorldStart) {
      TriggerEvent evt(wrldStartup);
      i->onTrigger(evt);
      }
  }

Item* WorldObjects::addItem(const ZenLoad::zCVobData &vob) {
  size_t inst = owner.getSymbolIndex(vob.oCItem.instanceName.c_str());
  Item*  it   = addItem(inst,nullptr);
  if(it==nullptr)
    return nullptr;

  Matrix4x4 m { vob.worldMatrix.mv };
  it->setMatrix(m);
  return it;
  }

Item *WorldObjects::takeItem(Item &it) {
  for(auto& i:itemArr)
    if(i.get()==&it){
      auto ret=i.release();
      i = std::move(itemArr.back());
      itemArr.pop_back();
      return ret;
      }
  return nullptr;
  }

void WorldObjects::removeItem(Item &it) {
  if(auto ptr=takeItem(it)){
    delete ptr;
    }
  }

size_t WorldObjects::hasItems(const std::string &tag, size_t itemCls) {
  for(auto& i:interactiveObj)
    if(i.tag()==tag) {
      return i.inventory().itemCount(itemCls);
      }
  return 0;
  }

Bullet& WorldObjects::shootBullet(size_t itmId, float x, float y, float z, float dx, float dy, float dz) {
  float speed=DynamicWorld::bulletSpeed;
  bullets.emplace_back(owner,itmId);
  auto& b = bullets.back();

  b.setPosition(x,y,z);
  b.setDirection(dx*speed,dy*speed,dz*speed);
  return b;
  }

Item *WorldObjects::addItem(size_t itemInstance, const char *at) {
  if(itemInstance==size_t(-1))
    return nullptr;

  auto  pos = owner.findPoint(at);

  std::unique_ptr<Item> ptr{new Item(owner,itemInstance)};
  auto* it=ptr.get();
  itemArr.emplace_back(std::move(ptr));

  if(pos!=nullptr) {
    it->setPosition (pos->x,pos->y,pos->z);
    it->setDirection(pos->dirX,pos->dirY,pos->dirZ);
    }

  auto& itData = *it->handle();
  it->setView(owner.getStaticView(itData.visual,itData.material));
  return it;
  }

void WorldObjects::addInteractive(const ZenLoad::zCVobData &vob) {
  interactiveObj.emplace_back(owner,vob);
  }

Interactive* WorldObjects::validateInteractive(Interactive *def) {
  return validateObj(interactiveObj,def);
  }

Npc *WorldObjects::validateNpc(Npc *def) {
  return validateObj(npcArr,def);
  }

Item *WorldObjects::validateItem(Item *def) {
  return validateObj(itemArr,def);
  }

Interactive* WorldObjects::findInteractive(const Npc &pl, Interactive* def, const Matrix4x4 &mvp,
                                           int w, int h, const SearchOpt& opt) {
  def = validateInteractive(def);
  if(def && testObjRange(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
    return nullptr;

  Interactive* ret  = nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  interactiveObj.find(pl.position(),opt.rangeMax,[&](Interactive& n){
    float nlen = rlen;
    if(testObj(n,pl,mvp,w,h,opt,nlen)){
      rlen = nlen;
      ret  = &n;
      }
    return false;
    });
  return ret;
  }

Npc* WorldObjects::findNpc(const Npc &pl, Npc *def, const Matrix4x4 &v, int w, int h, const SearchOpt& opt) {
  def = validateNpc(def);
  if(def) {
    auto xopt  = opt;
    xopt.flags = SearchFlg(xopt.flags | SearchFlg::NoAngle | SearchFlg::NoRay);
    if(def && testObjRange(*def,pl,xopt))
      return def;
    }
  auto r = findObj(npcArr,pl,v,w,h,opt);
  return r ? r->get() : nullptr;
  }

Item *WorldObjects::findItem(const Npc &pl, Item *def, const Matrix4x4 &mvp, int w, int h, const SearchOpt& opt) {
  def = validateObj(itemArr,def);
  if(def && testObjRange(*def,pl,opt))
    return def;
  if(owner.view()==nullptr)
    return nullptr;

  Item* ret  = nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  itemArr.find(pl.position(),opt.rangeMax,[&](std::unique_ptr<Item>& n){
    float nlen = rlen;
    if(testObj(n,pl,mvp,w,h,opt,nlen)){
      rlen = nlen;
      ret  = n.get();
      }
    return false;
    });
  return ret;
  }

void WorldObjects::marchInteractives(Tempest::Painter &p,const Tempest::Matrix4x4& mvp,
                                     int w,int h) const {
  for(auto& i:interactiveObj){
    auto m = mvp;
    m.mul(i.objMat);

    float x=0,y=0,z=0;
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = (0.5f*x+0.5f)*w;
    y = (0.5f*y+0.5f)*h;

    p.setBrush(Tempest::Color(1,1,1,1));
    p.drawRect(int(x),int(y),1,1);

    i.marchInteractives(p,mvp,w,h);
    }
  }

Interactive *WorldObjects::aviableMob(const Npc &pl, const std::string &dest) {
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

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.x      = self.position()[0];
  m.y      = self.position()[1];
  m.z      = self.position()[2];
  m.self   = &self;
  m.other  = &other;
  m.victum = &victum;

  sndPerc.push_back(m);
  }

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc &victum, Item &itm, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.x      = self.position()[0];
  m.y      = self.position()[1];
  m.z      = self.position()[2];
  m.self   = &self;
  m.other  = &other;
  m.victum = &victum;
  m.item   = itm.handle()->instanceSymbol;

  sndPerc.push_back(m);
  }

void WorldObjects::resetPositionToTA() {
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
  }

template<class T>
T& deref(std::unique_ptr<T>& x){ return *x; }

template<class T>
T& deref(T& x){ return x; }

template<class T>
bool checkFlag(T&,WorldObjects::SearchFlg){ return true; }

static bool checkFlag(Npc& n,WorldObjects::SearchFlg f){
  if( bool(f&WorldObjects::NoDeath) && n.isDead())
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
  return pl.canSeeNpc(n.position()[0],n.position()[1]+20,n.position()[2],true);
  }

template<class T>
auto WorldObjects::findObj(T &src,const Npc &pl, const Matrix4x4 &mvp,
                           int w, int h,const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type* {
  typename std::remove_reference<decltype(src[0])>::type* ret=nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;
  if(owner.view()==nullptr)
    return nullptr;

  if(opt.collectAlgo==TARGET_COLLECT_NONE || opt.collectAlgo==TARGET_COLLECT_CASTER)
    return nullptr;

  for(auto& n:src){
    float nlen = rlen;
    if(testObj(n,pl,mvp,w,h,opt,nlen)){
      rlen = nlen;
      ret=&n;
      }
    }
  return ret;
  }

template<class T>
bool WorldObjects::testObjRange(T &src, const Npc &pl, const WorldObjects::SearchOpt &opt) {
  float rlen = opt.rangeMax*opt.rangeMax;
  return testObjRange(src,pl,opt,rlen);
  }

template<class T>
bool WorldObjects::testObjRange(T &src, const Npc &pl, const WorldObjects::SearchOpt &opt,float& rlen){
  const float qmax  = opt.rangeMax*opt.rangeMax;
  const float qmin  = opt.rangeMin*opt.rangeMin;
  const float plAng = pl.rotationRad()+float(M_PI/2);
  const float ang   = float(std::cos(double(opt.azi)*M_PI/180.0));

  auto& npc=deref(src);
  if(reinterpret_cast<void*>(&npc)==reinterpret_cast<const void*>(&pl))
    return false;

  if(!checkFlag(npc,opt.flags))
    return false;

  auto pos = npc.position();
  float dx=pl.position()[0]-pos[0];
  float dy=pl.position()[1]-pos[1];
  float dz=pl.position()[2]-pos[2];

  float l = (dx*dx+dy*dy+dz*dz);
  if(l>qmax || l<qmin)
    return false;

  auto angle=std::atan2(dz,dx);
  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  l = std::sqrt(dx*dx+dy*dy+dz*dz);
  if(l<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
    rlen=l;
    return true;
    }
  return false;
  }

template<class T>
bool WorldObjects::testObj(T &src, const Npc &pl, const Matrix4x4 &mvp,
                           int w, int h, const WorldObjects::SearchOpt &opt,float& rlen) {
  float l=rlen;
  if(!testObjRange(src,pl,opt,l))
    return false;

  auto& npc = deref(src);
  auto  pos = npc.position();
  float x   = pos[0],y=pos[1],z=pos[2];
  mvp.project(x,y,z);

  if(z<0.f)
    return false;

  x = 0.5f*x*w;
  y = 0.5f*y*h;

  if(l<rlen && canSee(pl,npc)){
    rlen=l;
    return true;
    }
  return false;
  }

template<class T, class E>
E *WorldObjects::validateObj(T &src, E *e) {
  if(e==nullptr)
    return e;
  for(auto& i:src){
    auto& npc=deref(i);
    if(reinterpret_cast<void*>(&npc)==reinterpret_cast<const void*>(e))
      return e;
    }
  return nullptr;
  }
