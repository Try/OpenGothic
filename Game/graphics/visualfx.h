#pragma once

#include <daedalus/DaedalusStdlib.h>

#include "game/constants.h"

class World;

class VisualFx final {
  public:
    VisualFx(Daedalus::GEngineClasses::CFx_Base&& src);

    const Daedalus::GEngineClasses::CFx_Base& handle() const { return fx; }
    const Daedalus::GEngineClasses::C_ParticleFXEmitKey& key(SpellFxKey type) const;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey&       key(SpellFxKey type);

    void emitSound(World& wrld, const std::array<float,3>& pos, SpellFxKey type) const;

  private:
    const Daedalus::GEngineClasses::CFx_Base      fx;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey keys[int(SpellFxKey::Count)];
  };

