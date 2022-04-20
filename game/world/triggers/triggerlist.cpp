#include "triggerlist.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

TriggerList::TriggerList(Vob* parent, World &world, ZenLoad::zCVobData &&d, Flags flags)
  :AbstractTrigger(parent,world,std::move(d),flags) {
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  if(data.zCTriggerList.list.size()==0)
    return;

  switch(data.zCTriggerList.listProcess) {
    case LP_ALL: {
      uint64_t offset = 0;
      for(auto& i:data.zCTriggerList.list) {
        offset += uint64_t(i.fireDelay*1000);
        uint64_t time = world.tickCount()+offset;
        TriggerEvent ex(i.triggerTarget,data.vobName,time,TriggerEvent::T_Trigger);
        world.execTriggerEvent(ex);
        }
      break;
      }
    case LP_NEXT: {
      auto& i = data.zCTriggerList.list[next];
      next = (next+1)%uint32_t(data.zCTriggerList.list.size());

      uint64_t time = world.tickCount()+uint64_t(i.fireDelay*1000);
      TriggerEvent ex(i.triggerTarget,data.vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
    case LP_RAND: {
      auto& i = data.zCTriggerList.list[world.script().rand(uint32_t(data.zCTriggerList.list.size()))];

      uint64_t time = world.tickCount()+uint64_t(i.fireDelay*1000);
      TriggerEvent ex(i.triggerTarget,data.vobName,time,TriggerEvent::T_Trigger);
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
