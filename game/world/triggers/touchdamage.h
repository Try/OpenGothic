#pragma once

#include "abstracttrigger.h"

class World;

class TouchDamage : public AbstractTrigger {
  public:
    TouchDamage(Vob* parent, World &world, ZenLoad::zCVobData&& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent &evt) override;
    void onIntersect(Npc& n) override;
    void tick(uint64_t dt) override;
    void takeDamage(Npc& npc, int32_t val, int32_t prot);

    uint64_t repeatTimeout = 0;
  };
