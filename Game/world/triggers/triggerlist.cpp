#include "triggerlist.h"

#include <Tempest/Log>

#include "world/world.h"

using namespace Tempest;

TriggerList::TriggerList(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  }

void TriggerList::onTrigger(const TriggerEvent& e) {
  //Log::d("{");
  //Log::d("trigger list: ",e.target);
  uint64_t offset = 0;
  for(auto& i:data.zCTriggerList.list) {
    offset += uint64_t(i.fireDelay*1000);
    uint64_t time = world.tickCount()+offset;
    TriggerEvent ex(i.triggerTarget,data.vobName,time,TriggerEvent::T_Trigger);
    world.execTriggerEvent(ex);
    }
  //Log::d("}");
  }
