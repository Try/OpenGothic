#include "triggerscript.h"

#include <Tempest/Log>
#include "world/world.h"

TriggerScript::TriggerScript(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup)
  :AbstractTrigger(parent,world,std::move(data),startup) {
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().runFunction(data.zCTriggerScript.scriptFunc.c_str());
    }
  catch(std::runtime_error& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
