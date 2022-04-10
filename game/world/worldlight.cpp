#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup)
  : Vob(parent,world,vob,startup,true) {
  light = LightGroup::Light(world,vob);
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
