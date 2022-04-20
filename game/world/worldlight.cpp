#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, ZenLoad::zCVobData&& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = LightGroup::Light(world,vob);
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
