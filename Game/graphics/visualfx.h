#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/Point>

#include "game/constants.h"
#include "pfxobjects.h"

class World;

class VisualFx final {
  public:
    VisualFx(Daedalus::GEngineClasses::CFx_Base&& src);

    const Daedalus::GEngineClasses::CFx_Base&            handle() const { return fx; }

    PfxObjects::Emitter                                  visual(World& owner) const;
    const Daedalus::GEngineClasses::C_ParticleFXEmitKey& key(SpellFxKey type) const;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey&       key(SpellFxKey type);
    const char*                                          origin() const { return emTrjOriginNode.c_str(); }

    void emitSound(World& wrld, const Tempest::Vec3& pos, SpellFxKey type) const;

  private:
    const Daedalus::GEngineClasses::CFx_Base      fx;
    std::string                                   emTrjOriginNode;
    Daedalus::GEngineClasses::C_ParticleFXEmitKey keys[int(SpellFxKey::Count)];
  };

