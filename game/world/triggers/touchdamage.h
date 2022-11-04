#pragma once

#include "abstracttrigger.h"

class World;

class TouchDamage : public AbstractTrigger {
  public:
    TouchDamage(Vob* parent, World &world, const phoenix::vobs::touch_damage& data, Flags flags);

  private:
    void onTrigger(const TriggerEvent &evt) override;
    void onIntersect(Npc& n) override;
    void tick(uint64_t dt) override;
    void takeDamage(Npc& npc, int32_t val, int32_t prot);

    uint64_t repeatTimeout = 0;
    bool     barrier = false;
    bool     blunt = false;
    bool     edge = false;
    bool     fire = false;
    bool     fly = false;
    bool     magic = false;
    bool     point = false;
    bool     fall = false;
    float    damage = 0;
    float    repeatDelaySec = 0;
  };
