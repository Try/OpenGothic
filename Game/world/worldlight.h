#pragma once

#include "vob.h"
#include "graphics/lightgroup.h"

class WorldLight : public Vob {
  public:
    WorldLight(Vob* parent, World& world, ZenLoad::zCVobData &&vob, bool startup);

  private:
    void  moveEvent() override;

    LightGroup::Light light;
  };

