#pragma once

#include "abstracttrigger.h"

class World;

class TriggerWorldStart : public AbstractTrigger {
  public:
    TriggerWorldStart(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
