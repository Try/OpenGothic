#include "zonetrigger.h"

#include "world/world.h"

ZoneTrigger::ZoneTrigger(ZenLoad::zCVobData &&data, World &owner)
  :Trigger(std::move(data),owner){
  }

bool ZoneTrigger::checkPos(float x, float y, float z) const {
  auto& b = data.bbox;
  if( b[0].x < x && x < b[1].x &&
      b[0].y < y && y < b[1].y &&
      b[0].z < z && z < b[1].z)
    return true;
  return false;
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    owner.changeWorld(data.oCTriggerChangeLevel.levelName,
                      data.oCTriggerChangeLevel.startVobName);
  }
