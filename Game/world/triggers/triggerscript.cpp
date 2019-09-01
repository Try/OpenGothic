#include "triggerscript.h"

#include "world/world.h"

TriggerScript::TriggerScript(ZenLoad::zCVobData&& data, World &owner)
  :AbstractTrigger(std::move(data),owner){
  }

void TriggerScript::onTrigger(const TriggerEvent &) {
  owner.script().runFunction(data.zCTriggerScript.scriptFunc.c_str());
  }
