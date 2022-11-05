#include "codemaster.h"

#include "world/world.h"
#include "game/serialize.h"

CodeMaster::CodeMaster(Vob* parent, World &world, const phoenix::vobs::code_master& cm, Flags flags)
  :AbstractTrigger(parent,world,cm,flags), keys(cm.slaves.size()) {
  target = cm.target;
  slaves = cm.slaves;
  ordered = cm.ordered;
  firstFalseIsFailure = cm.first_false_is_failure;
  failureTarget = cm.failure_target;
  }

void CodeMaster::onTrigger(const TriggerEvent &evt) {
  size_t count = 0;
  for(size_t i=0;i<keys.size();++i) {
    if(!keys[i])
      break;
    ++count;
    }

  for(size_t i=0;i<keys.size();++i) {
    if(slaves[i]==evt.emitter) {
      if(ordered && (count!=i)) {
        if(firstFalseIsFailure)
          onFailure();
        zeroState();
        return;
        }
      keys[i] = true;
      }
    }

  for(auto i:keys)
    if(!i) {
      return;
      }

  zeroState();
  TriggerEvent e(target,vobName,TriggerEvent::T_Activate);
  world.triggerEvent(e);
  }

void CodeMaster::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(keys);
  }

void CodeMaster::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(keys);
  }

void CodeMaster::onFailure() {
  if(!failureTarget.empty()) {
    TriggerEvent e(failureTarget,vobName,TriggerEvent::T_Activate);
    world.triggerEvent(e);
    }
  }

void CodeMaster::zeroState() {
  for(size_t i=0;i<keys.size();++i)
    keys[i] = false;
  }
