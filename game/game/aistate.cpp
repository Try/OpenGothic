#include "aistate.h"

#include "game/gamescript.h"

AiState::AiState(GameScript& owner,size_t id) {
  auto* fn = owner.findSymbol(id);
  if (fn != nullptr) {
    mname    = fn->name().c_str();

    funcIni  = id;
    funcLoop = owner.findSymbolIndex(fn->name() + "_Loop");
    funcEnd  = owner.findSymbolIndex(fn->name() + "_End");
    } else {
    mname    = "";

    funcIni  = id;
    funcLoop = uint32_t(-1);
    funcEnd  = uint32_t(-1);
    }
  }
