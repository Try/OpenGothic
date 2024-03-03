#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(Vob* parent, World &world, const zenkit::VirtualObject& d, Flags flags)
  :AbstractTrigger(parent,world,d,flags) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }
