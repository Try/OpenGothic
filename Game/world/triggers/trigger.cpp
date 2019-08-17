#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&data, World &owner)
  :data(std::move(data)), owner(owner) {
  }

ZenLoad::zCVobData::EVobType Trigger::vobType() const {
  return data.vobType;
  }

const std::string &Trigger::name() const {
  return data.vobName;
  }

void Trigger::onTrigger(const TriggerEvent&) {
  if(data.objectClass=="zCTrigger:zCVob"){
    TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName);
    owner.triggerEvent(e);
    return;
    }
  Log::d("TODO: trigger[",name(),";",data.objectClass,"]");
  }

void Trigger::onIntersect(Npc &) {
  }

bool Trigger::hasVolume() const {
  auto& b = data.bbox;
  if( b[0].x < b[1].x &&
      b[0].y < b[1].y &&
      b[0].z < b[1].z)
    return true;
  return false;
  }

bool Trigger::checkPos(float,float,float) const{
  return false;
  }
