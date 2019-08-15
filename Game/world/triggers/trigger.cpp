#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&data, World &owner)
  :data(std::move(data)), owner(owner) {
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

bool Trigger::checkPos(float,float,float) const{
  return false;
  }
