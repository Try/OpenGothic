#include "visualfx.h"

#include "world/world.h"

VisualFx::VisualFx(Daedalus::GEngineClasses::CFx_Base &&src):fx(std::move(src)) {
  }

const Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type) const {
  return keys[int(type)];
  }

Daedalus::GEngineClasses::C_ParticleFXEmitKey& VisualFx::key(SpellFxKey type) {
  return keys[int(type)];
  }

void VisualFx::emitSound(World& wrld, const std::array<float,3>& pos, SpellFxKey type) const {
  auto& k   = key(type);
  float x   = pos[0];
  float y   = pos[1];
  float z   = pos[2];
  wrld.emitSoundEffect(k.sfxID.c_str(),x,y,z,0,true);
  }
