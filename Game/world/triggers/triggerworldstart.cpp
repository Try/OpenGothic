#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(World &owner, ZenLoad::zCVobData&& data, bool startup)
  :AbstractTrigger(owner,std::move(data),startup){
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(data.oCTriggerWorldStart.fireOnlyFirstTime && !ev.wrldStartup)
    return;

  TriggerEvent e(data.oCTriggerWorldStart.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
