#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/Point>

#include "game/constants.h"
#include "graphics/pfx/pfxobjects.h"

class World;

class VisualFx final {
  public:
    VisualFx(Daedalus::GEngineClasses::CFx_Base&& src, Daedalus::DaedalusVM& tmpVm, const char* name);

    enum Collision : uint8_t {
      NoCollide  = 0,
      Collide    = 1,
      Create     = 1<<1,
      CreateOnce = 1<<2,
      NoResp     = 1<<3,
      CreateQuad = 1<<4,
      };

    const Daedalus::GEngineClasses::CFx_Base&            handle() const { return fx; }

    Collision                                            actionColStat() const { return colStatFlg; }
    Collision                                            actionColDyn()  const { return colDynFlg; }
    const char*                                          colStat()       const;
    const char*                                          colDyn()        const;
    const char*                                          origin()        const { return emTrjOriginNode.c_str(); }

    PfxEmitter                                           visual(World& owner) const;
    const Daedalus::GEngineClasses::C_ParticleFXEmitKey& key(SpellFxKey type, int32_t keyLvl=0) const;

  private:
    const Daedalus::GEngineClasses::CFx_Base      fx;
    std::string                                   emTrjOriginNode;
    Collision                                     colStatFlg = NoCollide;
    Collision                                     colDynFlg  = NoCollide;

    static Collision                              strToColision(const char* s);

    Daedalus::GEngineClasses::C_ParticleFXEmitKey keys[int(SpellFxKey::Count)];
    std::vector<Daedalus::GEngineClasses::C_ParticleFXEmitKey> investKeys;
  };

