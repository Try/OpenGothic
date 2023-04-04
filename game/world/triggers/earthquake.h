#pragma once

#include <phoenix/vobs/camera.hh>
#include "abstracttrigger.h"

class World;

class Earthquake : public AbstractTrigger {
  public:
    Earthquake(Vob* parent, World& world, const phoenix::vobs::earthquake& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:

  };
