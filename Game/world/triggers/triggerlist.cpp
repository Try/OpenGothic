#include "triggerlist.h"

#include "world/world.h"

TriggerList::TriggerList(ZenLoad::zCVobData &&d, World &w)
  :AbstractTrigger(std::move(d),w) {
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  for(auto& i:data.zCTriggerList.list) {
    uint64_t time = owner.tickCount()+uint64_t(i.fireDelay*1000);
    TriggerEvent e(i.triggerTarget,data.vobName,time);
    owner.triggerEvent(e);
    }
  }
