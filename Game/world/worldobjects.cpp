#include "worldobjects.h"

#include "item.h"
#include "npc.h"
#include "world.h"

#include <Tempest/Painter>

using namespace Tempest;
using namespace Daedalus::GameState;

WorldObjects::WorldObjects(World& owner):owner(owner){
  }

WorldObjects::~WorldObjects() {
  }

void WorldObjects::tick(uint64_t dt) {
  for(auto& i:npcArr){
    i->tick(dt);
    }
  auto pl = owner.player();
  if(pl!=nullptr) {
    for(auto& i:npcArr){
      if(i.get()!=pl)
        i->preceptionPlayer(*pl);
      }
    }
  }

void WorldObjects::onInserNpc(NpcHandle handle,const std::string& point) {
  auto  pos = owner.findPoint(point);
  if(pos==nullptr){
    Log::e("onInserNpc: invalid waypoint");
    }
  auto  hnpc    = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData = owner.script()->getGameState().getNpc(hnpc);

  std::unique_ptr<Npc> ptr{new Npc(*owner.script(),hnpc)};
  npcData.userPtr = ptr.get();
  if(!npcData.name[0].empty())
    ptr->setName(npcData.name[0]);

  if(pos!=nullptr) {
    ptr->setPosition (pos->position.x,pos->position.y,pos->position.z);
    ptr->setDirection(pos->direction.x,
                      pos->direction.y,
                      pos->direction.z);
    }

  npcArr.emplace_back(std::move(ptr));
  }

void WorldObjects::onRemoveNpc(NpcHandle handle) {
  // TODO: clear other weak-references
  const Npc* p = owner.script()->getNpc(handle);
  for(size_t i=0;i<npcArr.size();++i)
    if(p==npcArr[i].get()){
      npcArr[i]=std::move(npcArr.back());
      npcArr.pop_back();
      return;
      }
  }

void WorldObjects::addTrigger(const ZenLoad::zCVobData &vob) {
  triggers.emplace_back(vob);
  }

const Trigger *WorldObjects::findTrigger(const char *name) const {
  if(name==nullptr || name[0]=='\0')
    return nullptr;
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
    owner.script()->getGameState().removeItem(ptr->handle());
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
  auto& itData = owner.script()->getGameState().getItem(h);

  std::unique_ptr<Item> ptr{new Item(*owner.script(),h)};
  auto* it=ptr.get();
  itData.userPtr = ptr.get();
  itData.amount  = 1;
  itemArr.emplace_back(std::move(ptr));

  if(pos!=nullptr) {
    it->setPosition (pos->position.x,pos->position.y,pos->position.z);
    it->setDirection(pos->direction.x,
                     pos->direction.y,
                     pos->direction.z);
    }

  it->setView(owner.getView(itData.visual,itData.material,0,itData.material));
  return it;
  }

void WorldObjects::addInteractive(const ZenLoad::zCVobData &vob) {
  interactiveObj.emplace_back(owner,vob);
  }

Interactive* WorldObjects::findInteractive(const Npc &pl, const Matrix4x4 &v, int w, int h,
                                           float rangeMin, float rangeMax, float azi) {
  auto r = findObj(interactiveObj,pl,rangeMin,rangeMax,azi,v,w,h);
  return r;
  }

Npc* WorldObjects::findNpc(const Npc &pl, const Matrix4x4 &v, int w, int h,
                           float rangeMin, float rangeMax, float azi) {
  auto r = findObj(npcArr,pl,rangeMin,rangeMax,azi,v,w,h);
  return r ? r->get() : nullptr;
  }

Item *WorldObjects::findItem(const Npc &pl, const Matrix4x4 &v, int w, int h,
                             float rangeMin, float rangeMax, float azi) {
  auto r = findObj(itemArr,pl,rangeMin,rangeMax,azi,v,w,h);
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
  const float dist=100*10.f;

  float x = pl.position()[0];
  float y = pl.position()[1];
  float z = pl.position()[2];

  for(auto& i:interactiveObj){
    float dx = i.position()[0]-x;
    float dy = i.position()[1]-y;
    float dz = i.position()[2]-z;

    if(dx*dx+dy*dy+dz*dz<dist*dist && i.isAvailable()) {
      auto name=i.focusName();
      if(name==dest)
        return &i;
      if(name.find("MOBNAME_")==0 && dest==name.c_str()+8)
        return &i;
      }
    }
  return nullptr;
  }

bool WorldObjects::aiUseMob(Npc &pl, const std::string &name) {
  auto inter = aviableMob(pl,name);
  if(inter==nullptr)
    return false;
  return pl.setInteraction(inter);
  }

template<class T>
T& deref(std::unique_ptr<T>& x){ return *x; }

template<class T>
T& deref(T& x){ return x; }

template<class T>
T* WorldObjects::findObj(std::vector<T> &src,const Npc &pl, float minDist, float maxDist,
                         float maxAngle, const Matrix4x4 &v, int w, int h) {
  T*    ret=nullptr;
  float rlen = w*h;
  if(owner.view()==nullptr)
    return nullptr;

  auto        mvp  = owner.view()->viewProj(v);
  const float qmax = maxDist*maxDist;
  const float qmin = minDist*minDist;

  const float ang   = float(std::cos(double(maxAngle)*M_PI/180.0));
  const float plAng = pl.rotationRad()+float(M_PI/2);

  for(auto& n:src){
    auto& npc=deref(n);
    if(reinterpret_cast<void*>(&npc)==reinterpret_cast<const void*>(&pl))
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

    l = std::sqrt(x*x+y*y);
    if(l<rlen){
      rlen=l;
      ret=&n;
      }
    }
  return ret;
  }
