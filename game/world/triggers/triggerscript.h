#pragma once

#include "abstracttrigger.h"

class World;

class TriggerScript : public AbstractTrigger {
  public:
    TriggerScript(Vob* parent, World& world, const std::unique_ptr<phoenix::vobs::vob>& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    std::string function;
  };
