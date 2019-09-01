#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&data, World &owner)
  :AbstractTrigger(std::move(data),owner) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
