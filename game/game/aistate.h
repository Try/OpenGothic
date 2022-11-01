#pragma once

#include <cstdlib>
#include <string>

class GameScript;

class AiState final {
  public:
    AiState(GameScript &owner, size_t id);

    size_t funcIni =0;
    size_t funcLoop=0;
    size_t funcEnd =0;

    const char* name() const { return mname; }
  private:
    const char* mname=""; //for debugging
  };
