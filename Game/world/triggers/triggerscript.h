#pragma once

#include "abstracttrigger.h"

class World;

class TriggerScript : public AbstractTrigger {
  public:
    TriggerScript(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
