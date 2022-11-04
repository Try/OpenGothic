#pragma once

#include "abstracttrigger.h"

class World;

class TriggerWorldStart : public AbstractTrigger {
  public:
    TriggerWorldStart(Vob* parent, World& world, const phoenix::vobs::trigger_world_start& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    bool fireOnlyFirstTime;
  };
