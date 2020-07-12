#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(World &w, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(w,std::move(d),startup) {
  }

void MessageFilter::onTrigger(const TriggerEvent&) {
  auto eval = data.zCMessageFilter.onTrigger;

  if(eval==ZenLoad::MutateType::MT_TRIGGER) {
    TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName);
    owner.triggerEvent(e);
    return;
    }

  if(eval==ZenLoad::MutateType::MT_ENABLE) {
    return;
    TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName);
    // owner.triggerEvent(e);
    }
  }
