#include "messagefilter.h"

#include "world/world.h"

MessageFilter::MessageFilter(ZenLoad::zCVobData &&data, World &owner)
  :AbstractTrigger(std::move(data),owner){
  }

void MessageFilter::onTrigger(const TriggerEvent&) {
  if(data.zCMessageFilter.onTrigger!=ZenLoad::MutateType::MT_ENABLE)
    return;
  TriggerEvent e(data.zCMessageFilter.triggerTarget,data.vobName);
  // owner.triggerEvent(e);
  }
