#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  }

void MessageFilter::onTrigger(const TriggerEvent&) {
  auto eval = data.zCMessageFilter.onTrigger;

  if(eval==ZenLoad::MutateType::MT_TRIGGER) {
    TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName);
    world.triggerEvent(e);
    return;
    }

  if(eval==ZenLoad::MutateType::MT_ENABLE) {
    return;
    TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName);
    // owner.triggerEvent(e);
    }
  }
