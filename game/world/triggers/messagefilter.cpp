#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(Vob* parent, World &world, const phoenix::vobs::message_filter& filt, Flags flags)
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

void MessageFilter::exec(phoenix::message_filter_action eval) {
  switch(eval) {
    case phoenix::message_filter_action::none:
      break;
    case phoenix::message_filter_action::trigger: {
      TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
      world.execTriggerEvent(e);
      break;
      }
    case phoenix::message_filter_action::untrigger: {
      TriggerEvent e(target,vobName,TriggerEvent::T_Untrigger);
      world.execTriggerEvent(e);
      break;
      }
    case phoenix::message_filter_action::enable: {
      TriggerEvent e(target,vobName,TriggerEvent::T_Enable);
      world.execTriggerEvent(e);
      break;
      }
    case phoenix::message_filter_action::disable:{
      TriggerEvent e(target,vobName,TriggerEvent::T_Disable);
      world.execTriggerEvent(e);
      break;
      }
    case phoenix::message_filter_action::toggle:{
      TriggerEvent e(target,vobName,TriggerEvent::T_ToggleEnable);
      world.execTriggerEvent(e);
      break;
      }
    }
  }
