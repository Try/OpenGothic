#pragma once

#include "vob.h"

class WorldLight : public Vob {
  public:
    WorldLight(Vob* parent, World& world, ZenLoad::zCVobData &&vob, bool startup);

  private:
    void  moveEvent() override;
  };

