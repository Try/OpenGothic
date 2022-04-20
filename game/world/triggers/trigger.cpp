#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(Vob* parent, World &world, ZenLoad::zCVobData &&d, Flags flags)
  :AbstractTrigger(parent,world,std::move(d),flags) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(data.zCTrigger.triggerTarget,data.vobName,TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }
