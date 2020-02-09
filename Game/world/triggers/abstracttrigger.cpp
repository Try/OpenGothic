#include "abstracttrigger.h"

#include <Tempest/Log>
#include "world/world.h"

using namespace Tempest;

AbstractTrigger::AbstractTrigger(ZenLoad::zCVobData &&data, World &owner)
  :data(std::move(data)), owner(owner) {
  }

ZenLoad::zCVobData::EVobType AbstractTrigger::vobType() const {
  return data.vobType;
  }

const std::string &AbstractTrigger::name() const {
  return data.vobName;
  }

void AbstractTrigger::processOnStart(const TriggerEvent& evt) {
  if(vobType()==ZenLoad::zCVobData::VT_oCTriggerWorldStart) {
    processEvent(evt);
    return;
    }

  enum ReactFlg:uint8_t {
    reactToOnTrigger = 1,
    reactToOnTouch   = 1<<1,
    reactToOnDamage  = 1<<2,
    respondToObject  = 1<<3,
    respondToPC      = 1<<4,
    respondToNPC     = 1<<5,
    startEnabled     = 1<<6,
    respondToVobName = 1<<7,
    };
  ReactFlg flags       = ReactFlg(data.zCTrigger.flags);
  ReactFlg filterFlags = ReactFlg(data.zCTrigger.filterFlags);
  if((flags&startEnabled) && (filterFlags&startEnabled)) {
    processEvent(evt);
    return;
    }
  }

void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  if(data.zCTrigger.numCanBeActivated>0 &&
     uint32_t(data.zCTrigger.numCanBeActivated)<=emitCount) {
    return;
    }
  ++emitCount;
  onTrigger(evt);
  }

void AbstractTrigger::onTrigger(const TriggerEvent&) {
  Log::d("TODO: trigger[",name(),";",data.objectClass,"]");
  }

void AbstractTrigger::onUntrigger(const TriggerEvent&) {
  }

void AbstractTrigger::onIntersect(Npc &n) {
  for(auto i:intersect)
    if(i==&n)
      return;
  intersect.push_back(&n);
  if(intersect.size()==1)
    enableTicks();

  TriggerEvent e(false);
  onTrigger(e);
  }

void AbstractTrigger::tick(uint64_t) {
  for(size_t i=0;i<intersect.size();) {
    Npc& npc = *intersect[i];
    auto pos = npc.position();
    if(!checkPos(pos[0],pos[1]+npc.translateY(),pos[2])) {
      intersect[i] = intersect.back();
      intersect.pop_back();

      TriggerEvent e(false);
      onUntrigger(e);
      } else {
      ++i;
      }
    }
  if(intersect.size()==0)
    disableTicks();
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
  owner.enableTicks(*this);
  }

void AbstractTrigger::disableTicks() {
  owner.disableTicks(*this);
  }
