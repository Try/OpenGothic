#include "zonetrigger.h"

#include "world/world.h"

ZoneTrigger::ZoneTrigger(ZenLoad::zCVobData &&d, World &w)
  :AbstractTrigger(std::move(d),w){
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    owner.changeWorld(data.oCTriggerChangeLevel.levelName,
                      data.oCTriggerChangeLevel.startVobName);
  }
