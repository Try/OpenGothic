#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(Vob* parent, World &world, ZenLoad::zCVobData &&d, Flags flags)
  :AbstractTrigger(parent,world,std::move(d),flags) {
  }

void MessageFilter::onTrigger(const TriggerEvent&) {
  exec(data.zCMessageFilter.onTrigger);
  }

void MessageFilter::onUntrigger(const TriggerEvent&) {
  exec(data.zCMessageFilter.onUntrigger);
  }

void MessageFilter::exec(ZenLoad::MutateType eval) {
  switch(eval) {
    case ZenLoad::MutateType::MT_NONE:
      break;
    case ZenLoad::MutateType::MT_TRIGGER: {
      TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName,TriggerEvent::T_Trigger);
      world.execTriggerEvent(e);
      break;
      }
    case ZenLoad::MutateType::MT_UNTRIGGER: {
      TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName,TriggerEvent::T_Untrigger);
      world.execTriggerEvent(e);
      break;
      }
    case ZenLoad::MutateType::MT_ENABLE: {
      TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName,TriggerEvent::T_Enable);
      world.execTriggerEvent(e);
      break;
      }
    case ZenLoad::MutateType::MT_DISABLE:{
      TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName,TriggerEvent::T_Disable);
      world.execTriggerEvent(e);
      break;
      }
    case ZenLoad::MutateType::MT_TOOGLE_ENABLED:{
      TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName,TriggerEvent::T_ToogleEnable);
      world.execTriggerEvent(e);
      break;
      }
    }
  }
