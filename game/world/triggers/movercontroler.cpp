#include "movercontroler.h"

#include "world/world.h"

MoverControler::MoverControler(Vob* parent, World &world, const std::unique_ptr<phoenix::vobs::vob>& d, Flags flags)
  :AbstractTrigger(parent,world,d,flags) {
  auto* ctrl = (const phoenix::vobs::mover_controller*) d.get();
  message = ctrl->message;
  key = ctrl->key;
  }

void MoverControler::onUntrigger(const TriggerEvent&) {
  }

void MoverControler::onTrigger(const TriggerEvent&) {
  TriggerEvent ex(target,vobName,0,TriggerEvent::T_Move);
  ex.move.msg = ZenLoad::MoverMessage(message);
  ex.move.key = int(key);
  world.execTriggerEvent(ex);
  }
