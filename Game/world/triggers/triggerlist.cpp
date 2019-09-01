#include "triggerlist.h"

#include "world/world.h"

TriggerList::TriggerList(ZenLoad::zCVobData &&data, World &owner)
  :AbstractTrigger(std::move(data),owner) {
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  for(auto& i:data.zCTriggerList.list) {
    // TODO: fireDelay
    TriggerEvent e(i.triggerTarget,data.vobName);
    owner.triggerEvent(e);
    }
  }
