#include "triggerlist.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

TriggerList::TriggerList(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
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
