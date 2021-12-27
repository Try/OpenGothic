#pragma once

#include <daedalus/DaedalusVM.h>

#include "scriptplugin.h"

class GameScript;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, Daedalus::DaedalusVM& vm);

    static bool isRequired(Daedalus::DaedalusVM& vm);
  };

