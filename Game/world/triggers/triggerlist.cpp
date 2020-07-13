#include "triggerlist.h"

#include "world/world.h"

TriggerList::TriggerList(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  for(auto& i:data.zCTriggerList.list) {
    uint64_t time = world.tickCount()+uint64_t(i.fireDelay*1000);
    TriggerEvent e(i.triggerTarget,data.vobName,time);
    world.triggerEvent(e);
    }
  }
