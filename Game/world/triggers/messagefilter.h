#pragma once

#include "abstracttrigger.h"

class MessageFilter : public AbstractTrigger {
  public:
    MessageFilter(Vob* parent, World& world, ZenLoad::zCVobData&& data, bool startup);

    void onTrigger(const TriggerEvent& evt) override;
  };
