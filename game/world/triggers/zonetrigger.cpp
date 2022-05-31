#include "zonetrigger.h"

#include "world/objects/npc.h"
#include "world/world.h"

ZoneTrigger::ZoneTrigger(Vob* parent, World &world, const std::unique_ptr<phoenix::vobs::vob>& d, Flags flags)
  :AbstractTrigger(parent,world,d,flags){
  auto* trig = (const phoenix::vobs::trigger_change_level*) d.get();
  levelName = trig->level_name;
  startVobName = trig->start_vob;
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    world.triggerChangeWorld(levelName, startVobName);
  }
