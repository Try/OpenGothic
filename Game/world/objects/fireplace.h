#pragma once

#include "interactive.h"

class FirePlace : public Interactive {
  public:
    FirePlace(Vob* parent, World& world, ZenLoad::zCVobData &&vob, bool startup);
  };

