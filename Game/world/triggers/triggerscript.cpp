#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(ZenLoad::zCVobData&& data, World &owner)
  :AbstractTrigger(std::move(data),owner){
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    owner.script().runFunction(data.zCTriggerScript.scriptFunc.c_str());
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
