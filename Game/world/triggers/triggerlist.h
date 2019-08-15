#pragma once
#include "trigger.h"

class World;

class TriggerList : public Trigger {
  public:
    TriggerList(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
