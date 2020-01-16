#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&d, World &w)
  :AbstractTrigger(std::move(d),w) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
