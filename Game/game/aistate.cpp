#include "aistate.h"

#include "world/worldscript.h"

AiState::AiState(GameScript& owner,size_t id) {
  auto& fn = owner.getSymbol(id);
  mname    = fn.name.c_str();

  funcIni  = id;
  funcLoop = owner.getSymbolIndex(fn.name+"_Loop");
  funcEnd  = owner.getSymbolIndex(fn.name+"_End");
  }
