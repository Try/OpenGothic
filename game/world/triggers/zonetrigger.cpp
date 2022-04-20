#include "zonetrigger.h"

#include "world/objects/npc.h"
#include "world/world.h"

ZoneTrigger::ZoneTrigger(Vob* parent, World &world, ZenLoad::zCVobData &&d, Flags flags)
  :AbstractTrigger(parent,world,std::move(d),flags){
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    world.triggerChangeWorld(data.oCTriggerChangeLevel.levelName,
                             data.oCTriggerChangeLevel.startVobName);
  }
