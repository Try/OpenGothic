#pragma once

#include "abstracttrigger.h"

class Trigger : public AbstractTrigger {
  public:
    Trigger(Vob* parent, World& world, ZenLoad::zCVobData&& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;
  };
