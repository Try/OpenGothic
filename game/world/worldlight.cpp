#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, const std::unique_ptr<phoenix::vob>& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = LightGroup::Light(world, (const phoenix::vobs::light&) *vob);
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
