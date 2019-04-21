#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <unordered_map>

class Gothic;

class SoundDefinitions final {
  public:
    SoundDefinitions(Gothic &gothic);

    const Daedalus::GEngineClasses::C_SFX& getSfx(const char* name);

  private:
    std::unordered_map<std::string,Daedalus::GEngineClasses::C_SFX> sfx;
  };

