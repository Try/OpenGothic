#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <memory>

class Gothic;

class MusicDefinitions final {
  public:
    MusicDefinitions(Gothic &gothic);
    ~MusicDefinitions();

    const Daedalus::GEngineClasses::C_MusicTheme* get(const char* name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    struct Theme : Daedalus::GEngineClasses::C_MusicTheme {
      size_t symId=0;
      };
    std::vector<Theme> themes;
  };
