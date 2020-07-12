#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(World &w, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(w,std::move(d),startup) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
