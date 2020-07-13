#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(Vob* parent, World& world, ZenLoad::zCVobData&& data, bool startup);

    void onIntersect(Npc& n) override;
  };
