#pragma once

#include <daedalus/DaedalusVM.h>

#include "scriptplugin.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, Daedalus::DaedalusVM& vm);

    static bool isRequired(Daedalus::DaedalusVM& vm);

  private:
    void  mem_readint        (Daedalus::DaedalusVM &vm);
    void  mem_writeint       (Daedalus::DaedalusVM &vm);
    void  mem_searchvobbyname(Daedalus::DaedalusVM &vm);

    void  call__stdcall      (Daedalus::DaedalusVM &vm);

    void  _deref      (Daedalus::DaedalusVM &vm);

    void  _takeref    (Daedalus::DaedalusVM &vm);
    void  _takeref_s  (Daedalus::DaedalusVM &vm);
    void  _takeref_f  (Daedalus::DaedalusVM &vm);

    // GameScript& owner;
  };

