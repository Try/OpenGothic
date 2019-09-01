#pragma once

#include "abstracttrigger.h"

class World;

class TriggerList : public AbstractTrigger {
  public:
    TriggerList(ZenLoad::zCVobData&& data, World &owner);

    void onTrigger(const TriggerEvent& evt) override;
  };
