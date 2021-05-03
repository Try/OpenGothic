#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup)
  :AbstractTrigger(parent,world,std::move(data),startup){
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(data.oCTriggerWorldStart.fireOnlyFirstTime && ev.type!=TriggerEvent::T_StartupFirstTime)
    return;

  TriggerEvent e(data.oCTriggerWorldStart.triggerTarget,data.vobName,TriggerEvent::T_Trigger);
  world.execTriggerEvent(e);
  }
