#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(World &owner, ZenLoad::zCVobData&& data, bool startup)
  :AbstractTrigger(owner,std::move(data),startup){
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    owner.script().runFunction(data.zCTriggerScript.scriptFunc.c_str());
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
