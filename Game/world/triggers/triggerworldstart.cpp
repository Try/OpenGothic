#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup)
  :AbstractTrigger(parent,world,std::move(data),startup){
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(data.oCTriggerWorldStart.fireOnlyFirstTime && !ev.wrldStartup)
    return;

  TriggerEvent e(data.oCTriggerWorldStart.triggerTarget,data.vobName);
  world.triggerEvent(e);
  }
