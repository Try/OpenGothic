#pragma once

#include "world/objects/vob.h"
#include "graphics/lightgroup.h"

class WorldLight : public Vob {
  public:
    WorldLight(Vob* parent, World& world, const phoenix::vobs::light& vob, Flags flags);

  private:
    void  moveEvent() override;

    LightGroup::Light light;
  };

