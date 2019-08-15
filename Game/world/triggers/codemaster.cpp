#include "codemaster.h"

#include "world/world.h"

CodeMaster::CodeMaster(ZenLoad::zCVobData&& data, World &owner)
  :Trigger(std::move(data),owner), keys(this->data.zCCodeMaster.slaveVobName.size()) {
  }

void CodeMaster::onTrigger(const TriggerEvent &evt) {
  for(size_t i=0;i<keys.size();++i){
    if(data.zCCodeMaster.slaveVobName[i]==evt.emitter)
      keys[i] = true;
    }

  for(auto i:keys)
    if(!i)
      return;

  TriggerEvent e(data.zCCodeMaster.triggerTarget,data.vobName);
  owner.triggerEvent(e);
  }
