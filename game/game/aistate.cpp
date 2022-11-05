#include "aistate.h"

#include "game/gamescript.h"

AiState::AiState(GameScript& owner,size_t id) {
  auto* fn = owner.getSymbol(id);
  if (fn != nullptr) {
    mname    = fn->name().c_str();

    funcIni  = id;
    funcLoop = owner.getSymbolIndex(fn->name() + "_Loop");
    funcEnd  = owner.getSymbolIndex(fn->name() + "_End");
    } else {
    mname    = "";

    funcIni  = id;
    funcLoop = -1;
    funcEnd  = -1;
    }
  }
