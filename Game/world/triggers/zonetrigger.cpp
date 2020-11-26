#include "zonetrigger.h"

#include "world/world.h"
#include "world/npc.h"

ZoneTrigger::ZoneTrigger(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup){
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    world.changeWorld(data.oCTriggerChangeLevel.levelName,
                      data.oCTriggerChangeLevel.startVobName);
  }
