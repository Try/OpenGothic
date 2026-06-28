#include "zonetrigger.h"

#include <zenkit/vobs/Trigger.hh>

#include "world/objects/npc.h"
#include "world/world.h"

ZoneTrigger::ZoneTrigger(Vob* parent, World &world, const zenkit::VTriggerChangeLevel& trig, Flags flags)
  :AbstractTrigger(parent,world,trig,flags){
  levelName = trig.level_name;
  startVobName = trig.start_vob;
  }

void ZoneTrigger::onIntersect(Npc &n) {
  if(!n.isPlayer())
    return;
  // NOTE: in original-game an oCTriggerChangeLevel touch runs through zCTrigger::OnTouch @0x00610640
  // -> zCTrigger::CanBeActivatedNow @0x00610220, whose first gate is the enabled flag (field_0x135 & 2):
  // a disabled change-level trigger (start_enabled==false, or disabled by a T_Disable event) returns 0
  // there and TriggerTarget @0x0043be20 is never reached, so no ChangeLevel happens. OpenGothic
  // activated the portal inline and ignored the enabled flag, teleporting the player through a disabled
  // portal. (T_Enable/T_Disable still flow through AbstractTrigger::processEvent, so the flag is live.)
  if(!isEnabled())
    return;
  // NOTE: in original-game oCTriggerChangeLevel::TriggerTarget @0x0043be20 the change-level is
  // aborted when the player has an active shapeshift/timed-effect spell (ids 0x2f..0x3a): it ends
  // the timed effect (transformBack) and returns WITHOUT calling oCGame::ChangeLevel. OpenGothic
  // teleported a transformed player straight through the portal, arriving still transformed.
  if(n.isTransformed()) {
    n.transformBack();
    return;
    }
  world.triggerChangeWorld(levelName, startVobName);
  }
