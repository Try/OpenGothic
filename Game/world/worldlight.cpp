#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent,world,vob,startup) {
  light = LightGroup::Light(world,vob);
  }

void WorldLight::moveEvent() {
  }
