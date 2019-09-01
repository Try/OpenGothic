#include "zonetrigger.h"

#include "world/world.h"

ZoneTrigger::ZoneTrigger(ZenLoad::zCVobData &&data, World &owner)
  :AbstractTrigger(std::move(data),owner){
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    owner.changeWorld(data.oCTriggerChangeLevel.levelName,
                      data.oCTriggerChangeLevel.startVobName);
  }
