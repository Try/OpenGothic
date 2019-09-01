#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(ZenLoad::zCVobData&& data, World &owner)
  :AbstractTrigger(std::move(data),owner){
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(data.oCTriggerWorldStart.fireOnlyFirstTime && !ev.wrldStartup)
    return;

  TriggerEvent e(data.oCTriggerWorldStart.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
