#pragma once

#include <daedalus/DaedalusVM.h>

#include "scriptplugin.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, Daedalus::DaedalusVM& vm);

    static bool isRequired(Daedalus::DaedalusVM& vm);

  private:
    void  mem_readint (Daedalus::DaedalusVM &vm);
    void  mem_writeInt(Daedalus::DaedalusVM &vm);

    GameScript& owner;
  };

