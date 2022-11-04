#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, const phoenix::vobs::light& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = LightGroup::Light(world, vob);
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
