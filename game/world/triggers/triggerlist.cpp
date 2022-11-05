#include "triggerlist.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

TriggerList::TriggerList(Vob* parent, World &world, const phoenix::vobs::trigger_list& list, Flags flags)
  :AbstractTrigger(parent,world,list,flags) {
  targets = list.targets;
  listProcess = list.mode;
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  if(targets.empty())
    return;

  switch(listProcess) {
    case phoenix::trigger_batch_mode::all: {
      uint64_t offset = 0;
      for(auto& i:targets) {
        offset += uint64_t(i.delay*1000);
        uint64_t time = world.tickCount()+offset;
        TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
        world.execTriggerEvent(ex);
        }
      break;
      }
    case phoenix::trigger_batch_mode::next: {
      auto& i = targets[next];
      next = (next+1)%uint32_t(targets.size());

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
    case phoenix::trigger_batch_mode::random: {
      auto& i = targets[world.script().rand(uint32_t(targets.size()))];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
    }
  }

void TriggerList::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(next);
  }

void TriggerList::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(next);
  }
