#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, const std::unique_ptr<phoenix::vobs::vob>& data, Flags flags)
  :AbstractTrigger(parent,world,data,flags) {
  function = ((const phoenix::vobs::trigger_script*) data.get())->function;
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().getVm().call_function(function);
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
