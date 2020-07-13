#pragma once

#include "abstracttrigger.h"

class World;

class TriggerWorldStart : public AbstractTrigger {
  public:
    TriggerWorldStart(Vob* parent, World& world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;
  };
