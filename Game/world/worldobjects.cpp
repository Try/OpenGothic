#include "worldobjects.h"

#include "graphics/staticobjects.h"
#include "item.h"
#include "npc.h"
#include "world.h"
#include "utils/workers.h"

#include <Tempest/Painter>
#include <Tempest/Application>

using namespace Tempest;
using namespace Daedalus::GameState;

WorldObjects::WorldObjects(World& owner):owner(owner){
  }

WorldObjects::~WorldObjects() {
  }

void WorldObjects::tick(uint64_t dt) {
  auto passive=std::move(sndPerc);
  sndPerc.clear();

  for(auto& i:npcArr)
    i->tick(dt);

  auto pl = owner.player();
  if(pl==nullptr)
    return;

  for(auto& i:npcArr) {
    if(i->percNextTime()>owner.tickCount())
      continue;
    const float x = i->position()[0];
    const float y = i->position()[1];
    const float z = i->position()[2];

    if(i.get()!=pl)
      i->perceptionProcess(*pl,pl->qDistTo(x,y,z));

    for(auto& r:passive) {
      float l = i->qDistTo(r.x,r.y,r.z);
      i->perceptionProcess(*r.other,r.victum,l,Npc::PercType(r.what));
      }
    }
  }

void WorldObjects::onInserNpc(Daedalus::GEngineClasses::C_Npc *handle, const std::string& point) {
  auto pos = owner.findPoint(point);
  if(pos==nullptr){
    Log::e("onInserNpc: invalid waypoint");
    }
  auto& npcData = *handle;

  std::unique_ptr<Npc> ptr{new Npc(*owner.script(),handle)};
  npcData.userPtr = ptr.get();
  if(!npcData.name[0].empty())
    ptr->setName(npcData.name[0]);

  if(pos!=nullptr && pos->isLocked()){
    auto p = owner.findNextPoint(*pos);
    if(p)
      pos=p;
    }
  if(pos!=nullptr) {
    ptr->setPosition  (pos->x,pos->y,pos->z);
    ptr->setDirection (pos->dirX,pos->dirY,pos->dirZ);
    ptr->attachToPoint(pos);
    ptr->updateTransform();
    }

  npcArr.emplace_back(std::move(ptr));
  }

void WorldObjects::updateAnimation() {
  Workers::parallelFor(npcArr,8,[](std::unique_ptr<Npc>& i){
    i->updateTransform();
    i->updateAnimation();
    });
  }

void WorldObjects::addTrigger(ZenLoad::zCVobData&& vob) {
  if(vob.vobType==ZenLoad::zCVobData::VT_zCMover){
    triggersMv.emplace_back(std::move(vob),owner);
    return;
    }

  triggers.emplace_back(std::move(vob));
  }

Trigger *WorldObjects::findTrigger(const char *name) {
  if(name==nullptr || name[0]=='\0')
    return nullptr;
  for(auto& i:triggersMv)
    if(i.name()==name)
      return &i;
  for(auto& i:triggers)
    if(i.name()==name)
      return &i;
  return nullptr;
  }

Item* WorldObjects::addItem(const ZenLoad::zCVobData &vob) {
  size_t inst = owner.getSymbolIndex(vob.oCItem.instanceName.c_str());
  Item*  it   = addItem(inst,nullptr);

  Matrix4x4 m;
  std::memcpy(&m,vob.worldMatrix.m,sizeof(m));
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

Item *WorldObjects::addItem(size_t itemInstance, const char *at) {
  auto  pos    = owner.findPoint(at);
  auto  h      = owner.script()->getGameState().insertItem(itemInstance);
  auto& itData = *h;

  std::unique_ptr<Item> ptr{new Item(*owner.script(),h)};
  auto* it=ptr.get();
  itData.userPtr = ptr.get();
  itData.amount  = 1;
  itemArr.emplace_back(std::move(ptr));

  if(pos!=nullptr) {
    it->setPosition (pos->x,pos->y,pos->z);
    it->setDirection(pos->dirX,pos->dirY,pos->dirZ);
    }

  it->setView(owner.getStaticView(itData.visual,itData.material));
  return it;
  }

void WorldObjects::addInteractive(const ZenLoad::zCVobData &vob) {
  interactiveObj.emplace_back(owner,vob);
  }

Interactive* WorldObjects::findInteractive(const Npc &pl, const Matrix4x4 &v, int w, int h, const SearchOpt& opt) {
  auto r = findObj(interactiveObj,pl,v,w,h,opt);
  return r;
  }

Npc* WorldObjects::findNpc(const Npc &pl, const Matrix4x4 &v, int w, int h, const SearchOpt& opt) {
  auto r = findObj(npcArr,pl,v,w,h,opt);
  return r ? r->get() : nullptr;
  }

Item *WorldObjects::findItem(const Npc &pl, const Matrix4x4 &v, int w, int h, const SearchOpt& opt) {
  auto r = findObj(itemArr,pl,v,w,h,opt);
  return r ? r->get() : nullptr;
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

bool WorldObjects::aiUseMob(Npc &pl, const std::string &name) {
  auto inter = aviableMob(pl,name);
  if(inter==nullptr)
    return false;
  return pl.setInteraction(inter);
  }

void WorldObjects::sendPassivePerc(Npc &self, Npc &other, Npc &victum, int32_t perc) {
  PerceptionMsg m;
  m.what   = perc;
  m.x      = self.position()[0];
  m.y      = self.position()[1];
  m.z      = self.position()[2];
  m.other  = &other;
  m.victum = &victum;

  sndPerc.push_back(m);
  }

void WorldObjects::resetPositionToTA() {
  for(auto& i:npcArr)
    i->resetPositionToTA();
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
  return pl.canSeeNpc(n.position()[0],n.position()[1]+100,n.position()[2],true);
  }

static bool canSee(const Npc& pl,const Item& n){
  return pl.canSeeNpc(n.position()[0],n.position()[1]+20,n.position()[2],true);
  }

template<class T>
auto WorldObjects::findObj(T &src,const Npc &pl, const Matrix4x4 &v,
                           int w, int h,const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type* {
  typename std::remove_reference<decltype(src[0])>::type* ret=nullptr;
  float rlen = opt.rangeMax*opt.rangeMax;//w*h;
  if(owner.view()==nullptr)
    return nullptr;

  auto        mvp  = owner.view()->viewProj(v);
  const float qmax = opt.rangeMax*opt.rangeMax;
  const float qmin = opt.rangeMin*opt.rangeMin;

  const float ang   = float(std::cos(double(opt.azi)*M_PI/180.0));
  const float plAng = pl.rotationRad()+float(M_PI/2);

  for(auto& n:src){
    auto& npc=deref(n);
    if(reinterpret_cast<void*>(&npc)==reinterpret_cast<const void*>(&pl))
      continue;
    if(!checkFlag(npc,opt.flags))
      continue;
    auto m = mvp;

    auto pos = npc.position();
    float dx=pl.position()[0]-pos[0];
    float dy=pl.position()[1]-pos[1];
    float dz=pl.position()[2]-pos[2];

    float l = (dx*dx+dy*dy+dz*dz);
    if(l>qmax || l<qmin)
      continue;

    auto angle=std::atan2(dz,dx);
    if(std::cos(plAng-angle)<ang)
      continue;

    float x=npc.position()[0],y=npc.position()[1],z=npc.position()[2];
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = 0.5f*x*w;
    y = 0.5f*y*h;

    l = std::sqrt(dx*dx+dy*dy+dz*dz);
    if(l<rlen && canSee(pl,npc)){
      rlen=l;
      ret=&n;
      }
    }
  return ret;
  }
