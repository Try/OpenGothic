#include "gsoundeffect.h"

void GSoundEffect::setOcclusion(float v) {
  occ = v;
  eff.setVolume(occ*vol);
  }

void GSoundEffect::setVolume(float v) {
  vol = v;
  eff.setVolume(occ*vol);
  }
