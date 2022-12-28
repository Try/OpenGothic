#include "movercontroler.h"

#include "world/world.h"

MoverControler::MoverControler(Vob* parent, World &world, const phoenix::vobs::mover_controller& ctrl, Flags flags)
  :AbstractTrigger(parent,world,ctrl,flags) {
  target = ctrl.target;
  message = ctrl.message;
  key = uint32_t(ctrl.key);
  }

void MoverControler::onUntrigger(const TriggerEvent&) {
  }

void MoverControler::onTrigger(const TriggerEvent&) {
  TriggerEvent ex(target,vobName,0,TriggerEvent::T_Move);
  ex.move.msg = message;
  ex.move.key = int(key);
  world.execTriggerEvent(ex);
  }
