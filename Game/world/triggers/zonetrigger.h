#pragma once

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(ZenLoad::zCVobData&& data, World &owner);

    void onIntersect(Npc& n) override;
  };
