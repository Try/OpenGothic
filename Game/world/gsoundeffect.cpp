#include "gsoundeffect.h"

GSoundEffect::GSoundEffect(Tempest::SoundEffect&& eff)
  :eff(std::move(eff)) {
  auto p = this->eff.position();
  pos = {p[0],p[1],p[2]};
  }

void GSoundEffect::setOcclusion(float v) {
  occ = v;
  eff.setVolume(occ*vol);
  }

void GSoundEffect::setVolume(float v) {
  vol = v;
  eff.setVolume(occ*vol);
  }
