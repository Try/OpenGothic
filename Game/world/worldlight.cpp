#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent,world,vob,startup) {
  light = world.getLight(vob);
  }

void WorldLight::moveEvent() {
  }
