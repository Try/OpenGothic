#include "worldlight.h"

#include "world.h"

WorldLight::WorldLight(Vob* parent, World& world, const zenkit::VLight& vob, Flags flags)
  : Vob(parent,world,vob,flags) {
  light = world.addLight(vob);
  if(!vob.is_static && !vob.on) {
    light.setEnabled(false);
    return;
    }

  if(vob.is_static && !vob.on) {
    //light.setEnabled(false);
    //return;
    }

  if(vob.is_static &&
     (vob.preset=="NW_STANDART_DARKBLUE" ||
      vob.preset=="DEFAULTLIGHT_DARKBLUE" || vob.preset=="INROOM_DARKBLUE")) {
    // ambient light-spam in many caves/houses
    light.setEnabled(false);
    }
  if(vob.is_static && vob.preset.find("AMBIENCE_")==0) {
    light.setEnabled(false);
    }
  if(vob.is_static && vob.preset.find("_STATIC")!=std::string::npos) {
    light.setEnabled(false);
    }
  if(vob.is_static && vob.preset=="SCHMUTZ") {
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
