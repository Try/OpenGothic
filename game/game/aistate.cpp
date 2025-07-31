#include "aistate.h"

#include "game/gamescript.h"

AiState::AiState(GameScript& owner,size_t id, bool g2) {
  auto* fn   = owner.findSymbol(id);
  auto& name = fn->name();

  funcIni = id;
  if(fn!=nullptr) {
    mname    = name.c_str();
    funcLoop = owner.findSymbolIndex(name + "_Loop");
    funcEnd  = owner.findSymbolIndex(name + "_End");

    playerEnabled = (name=="ZS_DEATH"  || name=="ZS_UNCONSCIOUS" || name=="ZS_MAGICFREEZE"     ||
                     name=="ZS_PYRO"   || name=="ZS_ASSESSMAGIC" || name=="ZS_ASSESSSTOPMAGIC" ||
                     name=="ZS_ZAPPED" || name=="ZS_SHORTZAPPED" || name=="ZS_MAGICSLEEP"      ||
                    (!g2 && name=="ZS_MAGICFEAR") ||
                    ( g2 && name=="ZS_WHIRLWIND"));
    } else {
    funcLoop = uint32_t(-1);
    funcEnd  = uint32_t(-1);
    }
  }
