#pragma once

#include "trigger.h"

class World;

class TriggerWorldStart : public Trigger {
  public:
    TriggerWorldStart(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
