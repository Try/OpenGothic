#include "trigger.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

Trigger::Trigger(Vob* parent, World &world, const zenkit::VirtualObject& d, Flags flags)
  :AbstractTrigger(parent,world,d,flags) {
  }

void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }

void Trigger::onUntrigger(const TriggerEvent&) {
  // NOTE: in original-game zCTrigger::UntriggerTarget @0x006103f0 a plain trigger relays an
  // OnUntrigger to its triggerTarget exactly as TriggerTarget @0x00610340 relays OnTrigger
  // (reached from OnUntrigger @0x00610600 / OnUntouch @0x00610660 after the react/enabled/
  // send-untrigger gate, already reproduced in AbstractTrigger::implProcessEvent). OpenGothic
  // overrode only onTrigger, so the gated untrigger hit the empty AbstractTrigger::onUntrigger and
  // was dropped -- targets reached through a plain trigger got the "on" half but never the "off".
  TriggerEvent e(target,vobName,TriggerEvent::T_Untrigger);
  world.triggerEvent(e);
  }
