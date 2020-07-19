#pragma once

#include "abstracttrigger.h"
#include "graphics/pfxobjects.h"

class World;

class PfxController : public AbstractTrigger {
  public:
    PfxController(Vob* parent, World& world, ZenLoad::zCVobData&& data, bool startup);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void moveEvent() override;
    void tick(uint64_t dt) override;

    PfxObjects::Emitter pfx;
    uint64_t            killed   = std::numeric_limits<uint64_t>::max();
    uint64_t            lifeTime = 0;
  };
