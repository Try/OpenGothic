#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(Vob* parent, World &world, const zenkit::VMessageFilter& filt, Flags flags)
  :AbstractTrigger(parent,world,filt,flags) {
  target = filt.target;
  onUntriggerA = filt.on_untrigger;
  onTriggerA = filt.on_trigger;
  }

void MessageFilter::onTrigger(const TriggerEvent&) {
  exec(onTriggerA);
  }

void MessageFilter::onUntrigger(const TriggerEvent&) {
  exec(onUntriggerA);
  }

void MessageFilter::exec(zenkit::MessageFilterAction eval) {
  switch(eval) {
    case zenkit::MessageFilterAction::none:
      break;
    case zenkit::MessageFilterAction::trigger: {
      TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_Trigger);
      world.execTriggerEvent(e);
      break;
      }
    case zenkit::MessageFilterAction::untrigger: {
      TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_Untrigger);
      world.execTriggerEvent(e);
      break;
      }
    case zenkit::MessageFilterAction::enable: {
      TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_Enable);
      world.execTriggerEvent(e);
      break;
      }
    case zenkit::MessageFilterAction::disable:{
      TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_Disable);
      world.execTriggerEvent(e);
      break;
      }
    case zenkit::MessageFilterAction::toggle:{
      TriggerEvent e(target,vobName,world.tickCount(),TriggerEvent::T_ToggleEnable);
      world.execTriggerEvent(e);
      break;
      }
    }
  }
