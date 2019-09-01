#pragma once

#include "graphics/staticobjects.h"
#include "physics/dynamicworld.h"
#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(ZenLoad::zCVobData&& data, World &owner);

    void onIntersect(Npc& n) override;
  };
