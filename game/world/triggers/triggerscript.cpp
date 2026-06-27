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
    auto& vm = world.script().getVm();
    // NOTE: in original-game oCTriggerScript::TriggerTarget @0x0043c4c0 the Daedalus context is
    // reset before the script call: SELF = activating vob cast to oCNpc (null for non-NPC
    // activators, i.e. the dominant trigger-chain case), OTHER = null, ITEM = null (not restored
    // afterwards). OpenGothic called the function with no context, so it saw stale self/other/item
    // from an unrelated prior call (commonly the player from a perception/dialog). Clear them.
    // (Setting SELF to the touching NPC on a direct-NPC-touch activation is deferred: onIntersect
    // drops the Npc pointer before onTrigger.)
    vm.global_self() ->set_instance(nullptr);
    vm.global_other()->set_instance(nullptr);
    vm.global_item() ->set_instance(nullptr);
    vm.call_function(function);
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
