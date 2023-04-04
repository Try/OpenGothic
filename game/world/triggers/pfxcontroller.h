#pragma once

#include "abstracttrigger.h"
#include "world/objects/pfxemitter.h"

class World;

class PfxController : public AbstractTrigger {
  public:
    PfxController(Vob* parent, World& world, const phoenix::vobs::pfx_controller& data, Flags flags);

    void save(Serialize &fout) const override;
    void load(Serialize &fin) override;

    void setActive(bool a);

  private:
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
    void moveEvent() override;
    void tick(uint64_t dt) override;

    PfxEmitter pfx;
    uint64_t   killed   = std::numeric_limits<uint64_t>::max();
    uint64_t   lifeTime = 0;
    bool       killWhenDone;
  };
