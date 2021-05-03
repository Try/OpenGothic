#include "movercontroler.h"

#include "world/world.h"

MoverControler::MoverControler(Vob* parent, World &world, ZenLoad::zCVobData &&d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  }

void MoverControler::onUntrigger(const TriggerEvent&) {
  }

void MoverControler::onTrigger(const TriggerEvent&) {
  TriggerEvent ex(data.zCMoverControler.triggerTarget,data.vobName,0,TriggerEvent::T_Move);
  ex.move.msg = data.zCMoverControler.moverMessage;
  ex.move.key = data.zCMoverControler.gotoFixedKey;
  world.execTriggerEvent(ex);
  }
