#pragma once

#include "trigger.h"

class World;

class TriggerScript : public Trigger {
  public:
    TriggerScript(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    World &owner;
  };
