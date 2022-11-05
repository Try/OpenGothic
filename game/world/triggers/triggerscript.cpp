#include <phoenix/vobs/trigger.hh>

#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, const phoenix::vobs::trigger_script& data, Flags flags)
  :AbstractTrigger(parent,world,data,flags) {
  function = data.function;
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().getVm().call_function(function);
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
