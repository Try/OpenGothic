#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, const zenkit::VLight& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = world.addLight(vob);
  if(!vob.is_static && !vob.on)
    light.setEnabled(false);
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
