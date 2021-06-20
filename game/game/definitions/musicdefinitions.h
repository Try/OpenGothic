#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <memory>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const Daedalus::GEngineClasses::C_MusicTheme* operator[](std::string_view name) const;

  private:
    std::unique_ptr<Daedalus::DaedalusVM>  vm;
    struct Theme : Daedalus::GEngineClasses::C_MusicTheme {
      size_t symId=0;
      };
    std::vector<Theme> themes;
  };
