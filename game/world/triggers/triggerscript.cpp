#include "triggerscript.h"

#include <zenkit/vobs/Trigger.hh>
#include <Tempest/Log>

#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, const zenkit::VTriggerScript& data, Flags flags)
  :AbstractTrigger(parent,world,data,flags) {
  function = data.function;
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().getVm().call_function(function);
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
