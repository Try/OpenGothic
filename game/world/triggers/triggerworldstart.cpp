#include <phoenix/vobs/trigger.hh>

#include "triggerworldstart.h"

#include "world/world.h"

TriggerWorldStart::TriggerWorldStart(Vob* parent, World &world, const std::unique_ptr<phoenix::vob>& data, Flags flags)
  :AbstractTrigger(parent,world,data,flags){
  fireOnlyFirstTime = ((const phoenix::vobs::trigger_world_start*) data.get())->fire_once;
  }

void TriggerWorldStart::onTrigger(const TriggerEvent &ev) {
  if(fireOnlyFirstTime && ev.type!=TriggerEvent::T_StartupFirstTime)
    return;

  TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
  world.execTriggerEvent(e);
  }
