#pragma once

#include "abstracttrigger.h"

class World;

class TriggerList : public AbstractTrigger {
  public:
    TriggerList(Vob* parent, World &world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;
  };
