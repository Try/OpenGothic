#pragma once

#include <phoenix/vm.hh>

#include "scriptplugin.h"

class GameScript;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, phoenix::vm& vm);

    static bool isRequired(phoenix::vm& vm);
  };

