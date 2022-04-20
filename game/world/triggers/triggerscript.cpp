#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, ZenLoad::zCVobData&& data, Flags flags)
  :AbstractTrigger(parent,world,std::move(data),flags) {
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().runFunction(data.zCTriggerScript.scriptFunc);
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
