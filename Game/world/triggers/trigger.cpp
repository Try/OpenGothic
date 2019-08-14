#include "trigger.h"

#include <Tempest/Log>

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&data)
  :data(std::move(data)) {
  }

const std::string &Trigger::name() const {
  return data.vobName;
  }

void Trigger::onTrigger(const TriggerEvent&) {
  Log::d("TODO: trigger[",name(),";",data.objectClass,"]");
  }

void Trigger::onIntersect(Npc &) {
  }

bool Trigger::checkPos(float,float,float) const{
  return false;
  }
