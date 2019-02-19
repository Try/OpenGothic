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

Item *WorldObjects::addItem(size_t itemInstance, const char *at) {
  auto  pos    = owner.findPoint(at);
  auto  h      = owner.script()->getGameState().insertItem(itemInstance);
  auto& itData = owner.script()->getGameState().getItem(h);

  std::unique_ptr<Item> ptr{new Item(*owner.script(),h)};
  auto* it=ptr.get();
  itData.userPtr = ptr.get();
  itemArr.emplace_back(std::move(ptr));

  if(pos!=nullptr) {
    it->setPosition (pos->position.x,pos->position.y,pos->position.z);
    it->setDirection(pos->direction.x,
                     pos->direction.y,
                     pos->direction.z);
    }

  it->setView(owner.getView(itData.visual));
  return it;
  }

void WorldObjects::addInteractive(const ZenLoad::zCVobData &vob) {
  interactiveObj.emplace_back(owner,vob);
  }

Interactive* WorldObjects::findInteractive(const Npc &pl, const Matrix4x4 &v, int w, int h) {
  Interactive* ret=nullptr;
  float rlen = w*h;
  if(owner.view()==nullptr)
    return nullptr;

  auto  mvp = owner.view()->viewProj(v);
  float plC = std::cos(float(M_PI/2)+pl.rotationRad());
  float plS = std::sin(float(M_PI/2)+pl.rotationRad());

  for(auto& i:interactiveObj){
    auto m = mvp;
    m.mul(i.objMat);

    auto pos = i.position();
    float dx=pl.position()[0]-pos[0];
    float dy=pl.position()[1]-pos[1];
    float dz=pl.position()[2]-pos[2];

    if(dx*plC+dy*plS<0)
      continue;
    float l = (dx*dx+dy*dy+dz*dz);
    if(l>220*220)
      continue;

    float x=0,y=0,z=0;
    m.project(x,y,z);

    if(z<0.f || z>1.f)
      continue;

    x = 0.5f*x*w;
    y = 0.5f*y*h;

    l = std::sqrt(x*x+y*y);
    if(l<rlen){
      rlen=l;
      ret=&i;
      }
    }
  return ret;
  }

Npc* WorldObjects::findNpc(const Npc &pl, const Matrix4x4 &v, int w, int h) {
  Npc* ret=nullptr;
  float rlen = w*h;
  if(owner.view()==nullptr)
    return nullptr;

  auto mvp = owner.view()->viewProj(v);

  for(auto& n:npcArr){
    auto& npc=*n;
    if(&npc==&pl)
      continue;
    auto m = mvp;
    //m.mul(npc.objMat);

    auto pos = npc.position();
    float dx=pl.position()[0]-pos[0];
    float dy=pl.position()[1]-pos[1];
    float dz=pl.position()[2]-pos[2];

    auto angle=std::atan2(dz,dx);
    if(std::sin(pl.rotationRad()-angle)>-float(std::sin(30*M_PI/180)))
      continue;
    float l = (dx*dx+dy*dy+dz*dz);
    if(l>440*440)
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
      ret=&npc;
      }
    }
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
