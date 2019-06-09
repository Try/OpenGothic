#pragma once

#include "graphics/staticobjects.h"
#include "physics/dynamicworld.h"
#include "trigger.h"

class ZoneTrigger : public Trigger {
  public:
    ZoneTrigger(ZenLoad::zCVobData&& data, World &owner);

    void onIntersect(Npc& n) override;
    bool checkPos(float x,float y,float z) const;

  private:
    World &owner;
  };
