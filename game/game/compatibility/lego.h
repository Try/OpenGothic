#pragma once

#include <phoenix/daedalus/interpreter.hh>

#include "scriptplugin.h"

class GameScript;

class LeGo : public ScriptPlugin {
  public:
    LeGo(GameScript& owner, phoenix::daedalus::vm& vm);

    static bool isRequired(phoenix::daedalus::vm& vm);
  };

