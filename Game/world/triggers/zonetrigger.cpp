#include "zonetrigger.h"

#include "world/world.h"

ZoneTrigger::ZoneTrigger(World &w, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(w,std::move(d),startup){
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    owner.changeWorld(data.oCTriggerChangeLevel.levelName,
                      data.oCTriggerChangeLevel.startVobName);
  }
