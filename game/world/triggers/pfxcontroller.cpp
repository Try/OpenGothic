#include "pfxcontroller.h"

#include "world/world.h"
#include "graphics/pfx/particlefx.h"
#include "game/serialize.h"
#include "gothic.h"

PfxController::PfxController(Vob* parent, World& world, ZenLoad::zCVobData&& d, bool startup)
  :AbstractTrigger(parent,world,std::move(d),startup) {
  auto& name = data.zCPFXControler.pfxName;
  const ParticleFx* view = Gothic::inst().loadParticleFx(name);
  if(view==nullptr)
    view = Gothic::inst().loadParticleFx(data.visual);
  if(view==nullptr)
    return;
  lifeTime = view->maxLifetime();
  pfx = PfxEmitter(world,view);
  pfx.setActive(d.zCPFXControler.pfxStartOn);
  pfx.setLooped(true);
  pfx.setObjMatrix(transform());
  }

void PfxController::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(killed,lifeTime,pfx.isActive());
  }

void PfxController::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  bool active = false;
  fin.read(killed,lifeTime,active);
  pfx.setActive(active);

  if(killed!=std::numeric_limits<uint64_t>::max())
    enableTicks();
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
