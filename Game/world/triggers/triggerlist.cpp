#include "triggerlist.h"

#include "world/world.h"

TriggerList::TriggerList(World &w, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(w,std::move(d),startup) {
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  for(auto& i:data.zCTriggerList.list) {
    uint64_t time = owner.tickCount()+uint64_t(i.fireDelay*1000);
    TriggerEvent e(i.triggerTarget,data.vobName,time);
    owner.triggerEvent(e);
    }
  }
