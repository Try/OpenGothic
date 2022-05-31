#pragma once

#include "abstracttrigger.h"

class World;

class TriggerWorldStart : public AbstractTrigger {
  public:
    TriggerWorldStart(Vob* parent, World& world, const std::unique_ptr<phoenix::vobs::vob>& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    bool fireOnlyFirstTime;
  };
