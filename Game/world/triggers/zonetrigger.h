#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(World &owner, ZenLoad::zCVobData&& data, bool startup);

    void onIntersect(Npc& n) override;
  };
