#include "abstracttrigger.h"

#include <Tempest/Log>
#include "world/world.h"

using namespace Tempest;

AbstractTrigger::AbstractTrigger(Vob* parent, World &world, ZenLoad::zCVobData &&data, bool startup)
  :Vob(parent,world,data,startup), data(std::move(data)), callback(this) {
  if(!hasFlag(StartEnabled))
    ;//disabled = true;
  world.addTrigger(this);
  box = world.physic()->bboxObj(&callback,data.bbox);
  }

AbstractTrigger::~AbstractTrigger() {
  if(box!=nullptr)
    world.physic()->deleteObj(box);
  }

ZenLoad::zCVobData::EVobType AbstractTrigger::vobType() const {
  return data.vobType;
  }

const std::string &AbstractTrigger::name() const {
  return data.vobName;
  }

bool AbstractTrigger::isEnabled() const {
  return !disabled;
  }

void AbstractTrigger::processOnStart(const TriggerEvent& evt) {
  if(vobType()==ZenLoad::zCVobData::VT_oCTriggerWorldStart) {
    processEvent(evt);
    return;
    }
  }

void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  //if(!hasFlag(ReactToOnTrigger))
  //  return;
  switch(evt.type) {
    case TriggerEvent::T_Trigger:
      if(disabled) {
        //Log::d("skip trigger: ",evt.target," [disabled]");
        return;
        }
      onTrigger(evt);
      break;
    case TriggerEvent::T_Untrigger:
      if(disabled) {
        //Log::d("skip trigger: ",evt.target," [disabled]");
        return;
        }
      onUntrigger(evt);
      break;
    case TriggerEvent::T_Enable:
      //Log::d("enable  trigger: ",evt.target);
      disabled = false;
      break;
    case TriggerEvent::T_Disable:
      disabled = true;
      //Log::d("disable trigger: ",evt.target);
      break;
    case TriggerEvent::T_ToogleEnable:
      disabled = !disabled;
      // if(disabled)
      //   Log::d("disable trigger: ",evt.target); else
      //   Log::d("enable  trigger: ",evt.target);
      break;
    case TriggerEvent::T_Activate: {
      const bool canActivate = (data.zCTrigger.numCanBeActivated<=0 ||
                                emitCount<uint32_t(data.zCTrigger.numCanBeActivated));
      if(canActivate) {
        ++emitCount;
        //Log::d("exec trigger: ",evt.target);
        onTrigger(evt);
        } else {
        //Log::d("skip trigger: ",evt.target," [emitCount]");
        }
      break;
      }
    }
  }

void AbstractTrigger::onTrigger(const TriggerEvent&) {
  Log::d("TODO: trigger[",name(),";",data.objectClass,"]");
  }

void AbstractTrigger::onUntrigger(const TriggerEvent&) {
  }

bool AbstractTrigger::hasFlag(ReactFlg flg) const {
  ReactFlg filter = ReactFlg(data.zCTrigger.flags & data.zCTrigger.filterFlags);
  return (filter&flg)==flg;
  }

void AbstractTrigger::onIntersect(Npc &n) {
  if(!hasFlag(n.isPlayer() ? RespondToPC : RespondToNPC) &&
     !hasFlag(ReactToOnTouch))
    return;

  if(!isEnabled())
    return;

  for(auto i:intersect)
    if(i==&n)
      return;
  intersect.push_back(&n);
  if(intersect.size()==1) {
    enableTicks();
    TriggerEvent e("","",TriggerEvent::T_Activate);
    processEvent(e);
    }
  }

void AbstractTrigger::tick(uint64_t) {
  for(size_t i=0;i<intersect.size();) {
    Npc& npc = *intersect[i];
    auto pos = npc.position();
    if(!checkPos(pos.x,pos.y+npc.translateY(),pos.z)) {
      intersect[i] = intersect.back();
      intersect.pop_back();
      } else {
      ++i;
      }
    }
  if(intersect.size()==0) {
    disableTicks();
    //TriggerEvent e("","",TriggerEvent::T_Untrigger);
    //processEvent(e);
    }
  }

bool AbstractTrigger::hasVolume() const {
  auto& b = data.bbox;
  if( b[0].x < b[1].x &&
      b[0].y < b[1].y &&
      b[0].z < b[1].z)
    return true;
  return false;
  }

bool AbstractTrigger::checkPos(float x,float y,float z) const{
  auto& b = data.bbox;
  if( b[0].x < x && x < b[1].x &&
      b[0].y < y && y < b[1].y &&
      b[0].z < z && z < b[1].z)
    return true;
  return false;
  }

void AbstractTrigger::enableTicks() {
  world.enableTicks(*this);
  }

void AbstractTrigger::disableTicks() {
  world.disableTicks(*this);
  }

void AbstractTrigger::Cb::onCollide(DynamicWorld::BulletBody&) {
  TriggerEvent ex(tg->data.vobName,tg->data.vobName,tg->world.tickCount(),TriggerEvent::T_Activate);
  tg->onTrigger(ex);
  }
