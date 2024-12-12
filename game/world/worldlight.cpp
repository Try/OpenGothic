#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, const zenkit::VLight& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = world.addLight(vob);
  if(!vob.is_static && !vob.on)
    light.setEnabled(false);

  if(vob.is_static && vob.quality==zenkit::LightQuality::LOW && vob.preset=="NW_STANDART_DARKBLUE") {
    // crazy light-spam in many caves
    light.setEnabled(false);
    }
  if(vob.is_static && vob.range>3000.0) {
    // unmotivated light blobs in old-mine ("MINE_DARK"/"MINE_DARKLIGHT")
    light.setEnabled(false);
    }
  }

void WorldLight::moveEvent() {
  light.setPosition(position());
  }
