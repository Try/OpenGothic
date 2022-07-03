#include "aistate.h"

#include "game/gamescript.h"

AiState::AiState(GameScript& owner,size_t id) {
  auto* fn = owner.getSymbol(id);
  mname    = fn->name();

  funcIni  = id;
  funcLoop = owner.getSymbolIndex(fn->name()+"_Loop");
  funcEnd  = owner.getSymbolIndex(fn->name()+"_End");
  }
