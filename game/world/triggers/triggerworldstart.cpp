#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(Vob* parent, World &world, ZenLoad::zCVobData&& data, Flags flags)
  :AbstractTrigger(parent,world,std::move(data),flags){
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(data.oCTriggerWorldStart.fireOnlyFirstTime && ev.type!=TriggerEvent::T_StartupFirstTime)
    return;

  TriggerEvent e(data.oCTriggerWorldStart.triggerTarget,data.vobName,TriggerEvent::T_Trigger);
  world.execTriggerEvent(e);
  }
