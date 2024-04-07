#include "codemaster.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"

CodeMaster::CodeMaster(Vob* parent, World &world, const zenkit::VCodeMaster& cm, Flags flags)
  :AbstractTrigger(parent,world,cm,flags), keys(cm.slaves.size()) {
  target              = cm.target;
  slaves              = cm.slaves;
  ordered             = cm.ordered;
  firstFalseIsFailure = cm.first_false_is_failure;
  failureTarget       = cm.failure_target;
  untriggeredCancels  = cm.untriggered_cancels;
  if(untriggeredCancels)
    Tempest::Log::d("zCCodeMaster::untriggeredCancels is not implemented. Vob: \"", vobName, "\"");
  }

void CodeMaster::onTrigger(const TriggerEvent &evt) {
  for(size_t i=0;i<keys.size();++i) {
    if(slaves[i]==evt.emitter) {
      if(!ordered || count==i)
        keys[i] = true;
      else if(firstFalseIsFailure) {
        onFailure();
        return;
        }
      ++count;
      break;
      }
    }
  if(count<keys.size())
    return;
  if(std::find(keys.begin(),keys.end(),false)!=keys.end()) {
    if(ordered)
      onFailure();
    } else {
      onSuccess();
    }
  }

void CodeMaster::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(keys,count);
  }

void CodeMaster::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(keys);
  if(fin.version()>48)
    fin.read(count);
  }

void CodeMaster::onFailure() {
  zeroState();
  if(!failureTarget.empty()) {
    TriggerEvent e(failureTarget,vobName,TriggerEvent::T_Trigger);
    world.triggerEvent(e);
    }
  }

void CodeMaster::onSuccess() {
  zeroState();
  if(!target.empty()) {
    TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
    world.triggerEvent(e);
    }
  }

void CodeMaster::zeroState() {
  for(size_t i=0;i<keys.size();++i)
    keys[i] = false;
  count = 0;
  }
