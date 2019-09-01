#pragma once

#include "abstracttrigger.h"

class Trigger : public AbstractTrigger {
  public:
    Trigger(ZenLoad::zCVobData&& data,World& owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
