#include "pfxcontroller.h"

#include "world/world.h"
#include "graphics/particlefx.h"

PfxController::PfxController(Vob* parent, World& world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  auto& name = data.zCPFXControler.pfxName;
  const ParticleFx* view = world.script().getParticleFx(name.c_str());
  if(view==nullptr)
    view = world.script().getParticleFx(data.visual.c_str());
  if(view==nullptr)
    return;
  lifeTime = view->maxLifetime();
  pfx = world.getView(view);
  pfx.setActive(d.zCPFXControler.pfxStartOn);
  pfx.setObjMatrix(transform());
  }

void PfxController::onTrigger(const TriggerEvent&) {
  if(killed<world.tickCount())
    return;
  pfx.setActive(true);
  if(data.zCPFXControler.killVobWhenDone) {
    killed = world.tickCount() + lifeTime;
    enableTicks();
    }
  }

void PfxController::onUntrigger(const TriggerEvent&) {
  pfx.setActive(false);
  }

void PfxController::moveEvent() {
  Vob::moveEvent();
  pfx.setObjMatrix(transform());
  }

void PfxController::tick(uint64_t /*dt*/) {
  if(killed<world.tickCount()) {
    disableTicks();
    pfx.setActive(false);
    }
  }
