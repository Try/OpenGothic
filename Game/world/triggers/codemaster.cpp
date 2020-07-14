#include "codemaster.h"

#include "world/world.h"

CodeMaster::CodeMaster(Vob* parent, World &world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup), keys(data.zCCodeMaster.slaveVobName.size()) {
  }

void CodeMaster::onTrigger(const TriggerEvent &evt) {
  for(size_t i=0;i<keys.size();++i) {
    if(data.zCCodeMaster.slaveVobName[i]==evt.emitter)
      keys[i] = true;
    }

  for(auto i:keys)
    if(!i)
      return;

  TriggerEvent e(data.zCCodeMaster.triggerTarget,data.vobName,TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }
